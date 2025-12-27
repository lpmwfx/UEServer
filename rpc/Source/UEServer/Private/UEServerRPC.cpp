// Copyright TwistedBrain. All Rights Reserved.

#include "UEServerRPC.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "HAL/RunnableThread.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

FUEServerRPC::FUEServerRPC()
	: ListenerSocket(nullptr)
	, Port(0)
	, bIsRunning(false)
	, Thread(nullptr)
{
	// Switchboard path: ~/.ueserver/switchboard.json
	SwitchboardPath = GetSwitchboardPath();

	// Project info
	ProjectPath = FPaths::GetProjectFilePath();
	if (ProjectPath.IsEmpty())
	{
		ProjectName = TEXT("UnrealEditor");
	}
	else
	{
		ProjectName = FPaths::GetBaseFilename(ProjectPath);
	}
}

FUEServerRPC::~FUEServerRPC()
{
	Stop();
}

bool FUEServerRPC::Start()
{
	if (bIsRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("UEServerRPC: Already running"));
		return false;
	}

	// Create listener socket
	if (!CreateListenerSocket())
	{
		UE_LOG(LogTemp, Error, TEXT("UEServerRPC: Failed to create listener socket"));
		return false;
	}

	// Register in switchboard
	if (!RegisterInSwitchboard())
	{
		UE_LOG(LogTemp, Error, TEXT("UEServerRPC: Failed to register in switchboard"));
		UnregisterFromSwitchboard();
		return false;
	}

	// Start server thread
	bIsRunning = true;
	Thread = FRunnableThread::Create(this, TEXT("UEServerRPC"), 0, TPri_Normal);

	return true;
}

void FUEServerRPC::Stop()
{
	if (!bIsRunning)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("UEServerRPC: Stopping..."));

	// Signal thread to stop
	bIsRunning = false;

	// Wait for thread to finish
	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}

	// Unregister from switchboard
	UnregisterFromSwitchboard();

	UE_LOG(LogTemp, Log, TEXT("UEServerRPC: Stopped"));
}

bool FUEServerRPC::Init()
{
	UE_LOG(LogTemp, Log, TEXT("UEServerRPC: Thread initialized"));
	return true;
}

uint32 FUEServerRPC::Run()
{
	UE_LOG(LogTemp, Log, TEXT("UEServerRPC: Server thread running on port %d"), Port);

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	while (bIsRunning)
	{
		// Check for pending connections (non-blocking)
		bool bHasPendingConnection = false;
		if (ListenerSocket->HasPendingConnection(bHasPendingConnection) && bHasPendingConnection)
		{
			// Accept connection
			FSocket* ClientSocket = ListenerSocket->Accept(TEXT("UEServerRPC Client"));
			if (ClientSocket != nullptr)
			{
				UE_LOG(LogTemp, Log, TEXT("UEServerRPC: Client connected"));
				HandleClient(ClientSocket);
			}
		}

		// Sleep briefly to avoid busy-waiting
		FPlatformProcess::Sleep(0.01f);
	}

	// Close listener socket
	if (ListenerSocket != nullptr)
	{
		ListenerSocket->Close();
		SocketSubsystem->DestroySocket(ListenerSocket);
		ListenerSocket = nullptr;
	}

	return 0;
}

void FUEServerRPC::Exit()
{
	UE_LOG(LogTemp, Log, TEXT("UEServerRPC: Thread exiting"));
}

bool FUEServerRPC::CreateListenerSocket()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSubsystem == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("UEServerRPC: No socket subsystem"));
		return false;
	}

	// Create socket
	ListenerSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UEServerRPC Listener"), false);
	if (ListenerSocket == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("UEServerRPC: Failed to create socket"));
		return false;
	}

	// Allow reuse
	ListenerSocket->SetReuseAddr(true);

	// Bind to localhost:0 (OS assigns available port)
	TSharedRef<FInternetAddr> LocalAddr = SocketSubsystem->CreateInternetAddr();
	bool bIsValid = false;
	LocalAddr->SetIp(TEXT("127.0.0.1"), bIsValid);
	if (!bIsValid)
	{
		UE_LOG(LogTemp, Error, TEXT("UEServerRPC: Invalid IP address"));
		SocketSubsystem->DestroySocket(ListenerSocket);
		ListenerSocket = nullptr;
		return false;
	}
	LocalAddr->SetPort(0); // Dynamic port allocation

	if (!ListenerSocket->Bind(*LocalAddr))
	{
		UE_LOG(LogTemp, Error, TEXT("UEServerRPC: Failed to bind socket"));
		SocketSubsystem->DestroySocket(ListenerSocket);
		ListenerSocket = nullptr;
		return false;
	}

	// Get the actual port assigned by OS
	TSharedRef<FInternetAddr> BoundAddr = SocketSubsystem->CreateInternetAddr();
	ListenerSocket->GetAddress(*BoundAddr);
	Port = BoundAddr->GetPort();

	// Start listening
	if (!ListenerSocket->Listen(8))
	{
		UE_LOG(LogTemp, Error, TEXT("UEServerRPC: Failed to listen on socket"));
		SocketSubsystem->DestroySocket(ListenerSocket);
		ListenerSocket = nullptr;
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("UEServerRPC: Listening on 127.0.0.1:%d"), Port);
	return true;
}

FString FUEServerRPC::GetSwitchboardPath() const
{
	// ~/.ueserver/switchboard.json
	FString UserHomeDir = FPlatformProcess::UserHomeDir();
	return FPaths::Combine(UserHomeDir, TEXT(".ueserver"), TEXT("switchboard.json"));
}

bool FUEServerRPC::RegisterInSwitchboard()
{
	// Create ~/.ueserver directory if needed
	FString UEServerDir = FPaths::GetPath(SwitchboardPath);
	if (!FPaths::DirectoryExists(UEServerDir))
	{
		if (!IFileManager::Get().MakeDirectory(*UEServerDir, true))
		{
			UE_LOG(LogTemp, Error, TEXT("UEServerRPC: Failed to create ~/.ueserver directory"));
			return false;
		}
	}

	// Read existing switchboard (or create new)
	TArray<TSharedPtr<FJsonValue>> Instances;
	if (FPaths::FileExists(SwitchboardPath))
	{
		FString JsonString;
		if (FFileHelper::LoadFileToString(JsonString, *SwitchboardPath))
		{
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
			if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* ExistingInstances = nullptr;
				if (JsonObject->TryGetArrayField(TEXT("instances"), ExistingInstances))
				{
					Instances = *ExistingInstances;
				}
			}
		}
	}

	// Remove stale entries for this PID (in case of crash recovery)
	int32 CurrentPid = FPlatformProcess::GetCurrentProcessId();
	Instances.RemoveAll([CurrentPid](const TSharedPtr<FJsonValue>& Value) {
		const TSharedPtr<FJsonObject>* ObjPtr;
		if (Value->TryGetObject(ObjPtr) && ObjPtr->IsValid())
		{
			int32 Pid = (*ObjPtr)->GetIntegerField(TEXT("pid"));
			return Pid == CurrentPid;
		}
		return false;
	});

	// Add new instance
	TSharedPtr<FJsonObject> NewInstance = MakeShared<FJsonObject>();
	NewInstance->SetNumberField(TEXT("pid"), CurrentPid);
	NewInstance->SetNumberField(TEXT("port"), Port);
	if (ProjectPath.IsEmpty())
	{
		NewInstance->SetField(TEXT("project"), MakeShared<FJsonValueNull>());
	}
	else
	{
		NewInstance->SetStringField(TEXT("project"), ProjectPath);
	}
	NewInstance->SetStringField(TEXT("project_name"), ProjectName);
	NewInstance->SetStringField(TEXT("started"), FDateTime::UtcNow().ToIso8601());

	Instances.Add(MakeShared<FJsonValueObject>(NewInstance));

	// Build switchboard JSON
	TSharedPtr<FJsonObject> Switchboard = MakeShared<FJsonObject>();
	Switchboard->SetArrayField(TEXT("instances"), Instances);

	// Serialize (compact JSON)
	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	if (!FJsonSerializer::Serialize(Switchboard.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("UEServerRPC: Failed to serialize switchboard JSON"));
		return false;
	}

	// Write to file
	if (!FFileHelper::SaveStringToFile(OutputString, *SwitchboardPath))
	{
		UE_LOG(LogTemp, Error, TEXT("UEServerRPC: Failed to write switchboard file"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("UEServerRPC: Registered in switchboard: %s (port %d)"), *ProjectName, Port);
	return true;
}

void FUEServerRPC::UnregisterFromSwitchboard()
{
	if (!FPaths::FileExists(SwitchboardPath))
	{
		return;
	}

	// Read switchboard
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *SwitchboardPath))
	{
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return;
	}

	// Remove this instance
	const TArray<TSharedPtr<FJsonValue>>* ExistingInstances = nullptr;
	if (!JsonObject->TryGetArrayField(TEXT("instances"), ExistingInstances))
	{
		return;
	}

	TArray<TSharedPtr<FJsonValue>> Instances = *ExistingInstances;
	int32 CurrentPid = FPlatformProcess::GetCurrentProcessId();

	Instances.RemoveAll([CurrentPid](const TSharedPtr<FJsonValue>& Value) {
		const TSharedPtr<FJsonObject>* ObjPtr;
		if (Value->TryGetObject(ObjPtr) && ObjPtr->IsValid())
		{
			int32 Pid = (*ObjPtr)->GetIntegerField(TEXT("pid"));
			return Pid == CurrentPid;
		}
		return false;
	});

	// Write back
	JsonObject->SetArrayField(TEXT("instances"), Instances);

	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
	{
		FFileHelper::SaveStringToFile(OutputString, *SwitchboardPath);
		UE_LOG(LogTemp, Log, TEXT("UEServerRPC: Unregistered from switchboard"));
	}
}

void FUEServerRPC::HandleClient(FSocket* ClientSocket)
{
	if (ClientSocket == nullptr)
	{
		return;
	}

	// Receive data
	const int32 BufferSize = 4096;
	uint8 Buffer[BufferSize];
	int32 BytesRead = 0;

	if (ClientSocket->Recv(Buffer, BufferSize - 1, BytesRead))
	{
		if (BytesRead > 0)
		{
			// Null-terminate and convert to FString
			Buffer[BytesRead] = 0;
			FString RequestJson = FString(UTF8_TO_TCHAR(Buffer));

			UE_LOG(LogTemp, Log, TEXT("UEServerRPC: Received request: %s"), *RequestJson);

			// Process request
			FString ResponseJson = ProcessRequest(RequestJson);

			// Send response
			const FTCHARToUTF8 Converter(*ResponseJson);
			int32 BytesSent = 0;
			ClientSocket->Send((const uint8*)Converter.Get(), Converter.Length(), BytesSent);

			UE_LOG(LogTemp, Log, TEXT("UEServerRPC: Sent response: %s"), *ResponseJson);
		}
	}

	// Close client socket
	ClientSocket->Close();
	ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
}

FString FUEServerRPC::ProcessRequest(const FString& RequestJson)
{
	// Parse JSON
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestJson);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		// Return error response
		TSharedPtr<FJsonObject> ErrorResponse = MakeShared<FJsonObject>();
		ErrorResponse->SetBoolField(TEXT("ok"), false);
		ErrorResponse->SetStringField(TEXT("error"), TEXT("Invalid JSON"));

		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(ErrorResponse.ToSharedRef(), Writer);
		return OutputString;
	}

	// Get operation
	FString Op = JsonObject->GetStringField(TEXT("op"));

	// Route to handler
	if (Op == TEXT("ping"))
	{
		return HandlePing(JsonObject);
	}
	else if (Op == TEXT("ui.get_tree"))
	{
		return HandleUIGetTree(JsonObject);
	}

	// Unknown operation
	TSharedPtr<FJsonObject> ErrorResponse = MakeShared<FJsonObject>();
	ErrorResponse->SetBoolField(TEXT("ok"), false);
	ErrorResponse->SetStringField(TEXT("op"), Op);
	ErrorResponse->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown operation: %s"), *Op));

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(ErrorResponse.ToSharedRef(), Writer);
	return OutputString;
}

FString FUEServerRPC::HandlePing(const TSharedPtr<FJsonObject>& Request)
{
	// Build response
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("ok"), true);
	Response->SetStringField(TEXT("op"), TEXT("ping"));
	Response->SetStringField(TEXT("version"), TEXT("0.1.0"));

	// Add id if present in request
	if (Request->HasField(TEXT("id")))
	{
		Response->SetStringField(TEXT("id"), Request->GetStringField(TEXT("id")));
	}

	// Serialize (compact, single-line JSON)
	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
	return OutputString;
}

FString FUEServerRPC::HandleUIGetTree(const TSharedPtr<FJsonObject>& Request)
{
	// Get optional parameters
	int32 MaxDepth = 10; // Default max depth
	if (Request->HasField(TEXT("max_depth")))
	{
		MaxDepth = Request->GetIntegerField(TEXT("max_depth"));
	}

	// Build response
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Add id if present in request
	if (Request->HasField(TEXT("id")))
	{
		Response->SetStringField(TEXT("id"), Request->GetStringField(TEXT("id")));
	}

	Response->SetStringField(TEXT("op"), TEXT("ui.get_tree"));

	// Check if Slate application is available
	if (!FSlateApplication::IsInitialized())
	{
		Response->SetBoolField(TEXT("ok"), false);
		Response->SetStringField(TEXT("error"), TEXT("Slate application not initialized"));

		FString OutputString;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
		FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
		return OutputString;
	}

	// Get all top-level windows
	TArray<TSharedRef<SWindow>> Windows;
	FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);

	// Serialize windows as array
	TArray<TSharedPtr<FJsonValue>> WindowsArray;
	for (const TSharedRef<SWindow>& Window : Windows)
	{
		TSharedPtr<FJsonObject> WindowObj = SerializeWidget(Window, MaxDepth, 0);
		if (WindowObj.IsValid())
		{
			WindowsArray.Add(MakeShared<FJsonValueObject>(WindowObj));
		}
	}

	Response->SetBoolField(TEXT("ok"), true);
	Response->SetArrayField(TEXT("windows"), WindowsArray);
	Response->SetNumberField(TEXT("window_count"), Windows.Num());

	// Serialize response
	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
	return OutputString;
}

TSharedPtr<FJsonObject> FUEServerRPC::SerializeWidget(TSharedPtr<SWidget> Widget, int32 MaxDepth, int32 CurrentDepth)
{
	if (!Widget.IsValid() || CurrentDepth >= MaxDepth)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> WidgetObj = MakeShared<FJsonObject>();

	// Widget type (class name)
	FString WidgetType = Widget->GetTypeAsString();
	WidgetObj->SetStringField(TEXT("type"), WidgetType);

	// Visibility
	EVisibility Visibility = Widget->GetVisibility();
	bool bIsVisible = Visibility.IsVisible();
	WidgetObj->SetBoolField(TEXT("visible"), bIsVisible);

	// Enabled state
	bool bIsEnabled = Widget->IsEnabled();
	WidgetObj->SetBoolField(TEXT("enabled"), bIsEnabled);

	// Geometry (size and position)
	FGeometry Geometry = Widget->GetCachedGeometry();
	TSharedPtr<FJsonObject> GeometryObj = MakeShared<FJsonObject>();
	FVector2D Size = Geometry.GetLocalSize();
	FVector2D AbsolutePosition = Geometry.GetAbsolutePosition();

	GeometryObj->SetNumberField(TEXT("x"), AbsolutePosition.X);
	GeometryObj->SetNumberField(TEXT("y"), AbsolutePosition.Y);
	GeometryObj->SetNumberField(TEXT("width"), Size.X);
	GeometryObj->SetNumberField(TEXT("height"), Size.Y);
	WidgetObj->SetObjectField(TEXT("geometry"), GeometryObj);

	// Try to get accessible text (for screen readers and debugging)
	FString AccessibleText = Widget->GetAccessibleText().ToString();
	if (!AccessibleText.IsEmpty())
	{
		WidgetObj->SetStringField(TEXT("text"), AccessibleText);
	}

	// Serialize children recursively
	FChildren* Children = Widget->GetChildren();
	if (Children && Children->Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ChildrenArray;
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			TSharedRef<SWidget> ChildWidget = Children->GetChildAt(i);
			TSharedPtr<FJsonObject> ChildObj = SerializeWidget(ChildWidget, MaxDepth, CurrentDepth + 1);
			if (ChildObj.IsValid())
			{
				ChildrenArray.Add(MakeShared<FJsonValueObject>(ChildObj));
			}
		}
		WidgetObj->SetArrayField(TEXT("children"), ChildrenArray);
		WidgetObj->SetNumberField(TEXT("child_count"), ChildrenArray.Num());
	}
	else
	{
		WidgetObj->SetNumberField(TEXT("child_count"), 0);
	}

	return WidgetObj;
}
