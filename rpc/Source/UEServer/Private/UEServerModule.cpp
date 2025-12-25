// Copyright TwistedBrain. All Rights Reserved.

#include "UEServer.h"
#include "UEServerRPC.h"

#define LOCTEXT_NAMESPACE "FUEServerModule"

void FUEServerModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("UEServer: Starting module..."));

	// Create and start RPC server
	RPCServer = MakeShared<FUEServerRPC>();
	if (RPCServer->Start())
	{
		UE_LOG(LogTemp, Log, TEXT("UEServer: RPC server started on port %d"), RPCServer->GetPort());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UEServer: Failed to start RPC server"));
	}
}

void FUEServerModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("UEServer: Shutting down module..."));

	// Stop RPC server
	if (RPCServer.IsValid())
	{
		RPCServer->Stop();
		RPCServer.Reset();
	}

	UE_LOG(LogTemp, Log, TEXT("UEServer: Module shut down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUEServerModule, UEServer)
