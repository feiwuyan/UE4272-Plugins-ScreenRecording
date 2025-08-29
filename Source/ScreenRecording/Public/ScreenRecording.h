// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSR, Log, All);

class FScreenRecordingModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void InitLibraryHandles();

	void UnloadHandledLibraries();

	void* LoadDependencyLibrary(const FString& DLLName);

	static void FFmpegCallback(void*, int Level, const char* Format, va_list ArgList);

	bool bInitialized;

	void* AVCodecHandle;
	void* AVDeviceHandle;
	void* AVFilterHandle;
	void* AVFormatHandle;
	void* AVResampleHandle;
	void* AVUtilHandle;
	void* LibMP3LameHandle;
	void* LibX264Handle;
	void* PostProcHandle;
	void* SWResampleHandle;
	void* SWScaleHandle;
};
