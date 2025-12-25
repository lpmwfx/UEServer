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

FUEServerRPC::FUEServerRPC()
	: ListenerSocket(nullptr)
	, Port(0)
	, bIsRunning(false)
	, Thread(nullptr)
{
	// Runtime state path: <ProjectDir>/.ueserver/rpc.json
	FString ProjectDir = FPaths::ProjectDir();
	RuntimeStatePath = FPaths::Combine(ProjectDir, TEXT(".ueserver"), TEXT("rpc.json"));
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

	// Write runtime state
	if (!WriteRuntimeState())
	{
		UE_LOG(LogTemp, Error, TEXT("UEServerRPC: Failed to write runtime state"));
		CleanupRuntimeState();
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

	// Cleanup
	CleanupRuntimeState();

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
	LocalAddr->SetIp(TEXT("127.0.0.1"));
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

bool FUEServerRPC::WriteRuntimeState()
{
	// Create .ueserver directory if it doesn't exist
	FString UEServerDir = FPaths::GetPath(RuntimeStatePath);
	if (!FPaths::DirectoryExists(UEServerDir))
	{
		if (!IFileManager::Get().MakeDirectory(*UEServerDir, true))
		{
			UE_LOG(LogTemp, Error, TEXT("UEServerRPC: Failed to create .ueserver directory"));
			return false;
		}
	}

	// Build JSON
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetNumberField(TEXT("port"), Port);
	JsonObject->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	JsonObject->SetStringField(TEXT("started"), FDateTime::UtcNow().ToIso8601());

	// Serialize to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("UEServerRPC: Failed to serialize runtime state JSON"));
		return false;
	}

	// Write to file
	if (!FFileHelper::SaveStringToFile(OutputString, *RuntimeStatePath))
	{
		UE_LOG(LogTemp, Error, TEXT("UEServerRPC: Failed to write runtime state file"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("UEServerRPC: Runtime state written to %s"), *RuntimeStatePath);
	return true;
}

void FUEServerRPC::CleanupRuntimeState()
{
	if (FPaths::FileExists(RuntimeStatePath))
	{
		IFileManager::Get().Delete(*RuntimeStatePath);
		UE_LOG(LogTemp, Log, TEXT("UEServerRPC: Removed runtime state file"));
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

	// Serialize
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
	return OutputString;
}
