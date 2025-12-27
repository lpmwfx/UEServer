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
 * - Register in ~/.ueserver/switchboard.json on startup
 * - Accept TCP connections and process JSON RPC requests
 * - Unregister from switchboard on shutdown
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

	/** Register this instance in switchboard */
	bool RegisterInSwitchboard();

	/** Unregister this instance from switchboard */
	void UnregisterFromSwitchboard();

	/** Get switchboard path (~/.ueserver/switchboard.json) */
	FString GetSwitchboardPath() const;

	/** Handle incoming client connection */
	void HandleClient(FSocket* ClientSocket);

	/** Process a single RPC request */
	FString ProcessRequest(const FString& RequestJson);

	/** RPC handler: ping operation */
	FString HandlePing(const TSharedPtr<FJsonObject>& Request);

	/** RPC handler: get UI widget tree */
	FString HandleUIGetTree(const TSharedPtr<FJsonObject>& Request);

	/** RPC handler: get specific widget by path */
	FString HandleUIGetWidget(const TSharedPtr<FJsonObject>& Request);

	/** Helper: Serialize widget to JSON recursively */
	TSharedPtr<FJsonObject> SerializeWidget(TSharedPtr<SWidget> Widget, int32 MaxDepth, int32 CurrentDepth);

	/** Helper: Find widget by path (e.g., "MainFrame/MenuBar/File") */
	TSharedPtr<SWidget> FindWidgetByPath(const FString& Path);

private:
	/** Listener socket */
	FSocket* ListenerSocket;

	/** Port the server is listening on */
	int32 Port;

	/** Server running flag */
	FThreadSafeBool bIsRunning;

	/** Server thread */
	FRunnableThread* Thread;

	/** Switchboard path (~/.ueserver/switchboard.json) */
	FString SwitchboardPath;

	/** Project path (can be empty if no project) */
	FString ProjectPath;

	/** Project name */
	FString ProjectName;
};
