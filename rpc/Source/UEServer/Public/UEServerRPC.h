// Copyright TwistedBrain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "HAL/Runnable.h"

/**
 * TCP-based RPC server for UEServer
 *
 * Responsibilities:
 * - Bind to dynamic port (OS assigns available port)
 * - Write .ueserver/rpc.json with port/pid/started on startup
 * - Accept TCP connections and process JSON RPC requests
 * - Clean up .ueserver/rpc.json on shutdown
 */
class UESERVER_API FUEServerRPC : public FRunnable
{
public:
	FUEServerRPC();
	virtual ~FUEServerRPC();

	/** Start the RPC server */
	bool Start();

	/** Stop the RPC server */
	void Stop();

	/** Get the port the server is listening on (0 if not started) */
	int32 GetPort() const { return Port; }

	/** Check if server is running */
	bool IsRunning() const { return bIsRunning; }

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;

private:
	/** Create and bind the listener socket */
	bool CreateListenerSocket();

	/** Write runtime state to .ueserver/rpc.json */
	bool WriteRuntimeState();

	/** Remove .ueserver/rpc.json */
	void CleanupRuntimeState();

	/** Handle incoming client connection */
	void HandleClient(FSocket* ClientSocket);

	/** Process a single RPC request */
	FString ProcessRequest(const FString& RequestJson);

	/** RPC handler: ping operation */
	FString HandlePing(const TSharedPtr<FJsonObject>& Request);

private:
	/** Listener socket */
	FSocket* ListenerSocket;

	/** Port the server is listening on */
	int32 Port;

	/** Server running flag */
	FThreadSafeBool bIsRunning;

	/** Server thread */
	FRunnableThread* Thread;

	/** Path to .ueserver/rpc.json */
	FString RuntimeStatePath;
};
