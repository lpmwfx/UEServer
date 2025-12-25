// Copyright TwistedBrain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * UEServer module - TCP-based RPC server for exposing UE UI controls
 */
class FUEServerModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** RPC server instance (created on module startup) */
	TSharedPtr<class FUEServerRPC> RPCServer;
};
