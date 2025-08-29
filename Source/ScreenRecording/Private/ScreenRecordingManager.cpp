// Fill out your copyright notice in the Description page of Project Settings.


#include "ScreenRecordingManager.h"

#include "RHICommandList.h"
#include "RenderingThread.h"
#include "RHIResources.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

void SRM_Listener::OnMediaSample(const AVEncoder::FMediaPacket& Sample)
{
	AScreenRecordingManager* CastOwner = Cast< AScreenRecordingManager>(Owner);
	CastOwner->OnMediaSample(Sample);
}

// Sets default values
AScreenRecordingManager::AScreenRecordingManager()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

AScreenRecordingManager::~AScreenRecordingManager()
{
	Stop();
}

// Called when the game starts or when spawned
void AScreenRecordingManager::BeginPlay()
{
	Super::BeginPlay();
	GME = FSRGameplayMediaEncoder::Get();
}

// Called every frame
void AScreenRecordingManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AScreenRecordingManager::Initialize()
{
	if (AsyncLock)
	{
		return;
	}

	TWeakObjectPtr<AScreenRecordingManager> WeakThis(this);

	AsyncTask(ENamedThreads::AnyThread, [WeakThis]()
		{
			// 检查 Actor 是否仍然有效
			if (WeakThis.IsValid())
			{
				WeakThis->AsyncLock = true;
				WeakThis->PerformAsyncInitialization();
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AMyActor was destroyed before async init could start."));
			}
		});
}

void AScreenRecordingManager::OnAsyncInitCompleted(bool bSuccess)
{
	bIsInitialize = bSuccess;

	OnInitCompleted.Broadcast(bSuccess);

	AsyncLock = false;
}

void AScreenRecordingManager::PerformAsyncInitialization()
{
	TWeakObjectPtr<AScreenRecordingManager> WeakThis(this);
	bool bSuccess = bIsInitialize;
	if (bIsInitialize)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->OnAsyncInitCompleted(bSuccess);
				}
			});
		return;
	}

	bIsInitialize = GME->Initialize();

	FString FilePath = FPaths::ProjectSavedDir() / "CapturedVideo.mp4";

	AVEncoder::FAudioConfig AudioConfig;
	AudioConfig.Codec = "aac";
	AudioConfig.Samplerate = 48000;
	AudioConfig.NumChannels = 2;
	AudioConfig.Bitrate = 192000;

	AVEncoder::FVideoConfig VideoConfig;
	VideoConfig.Codec = "h264";
	VideoConfig.Height = VideoConfig.Width = VideoConfig.Framerate = VideoConfig.Bitrate = 0;
	FParse::Value(FCommandLine::Get(), TEXT("GameplayMediaEncoder.ResY="), VideoConfig.Height);
	UE_LOG(LogTemp, Log, TEXT("GameplayMediaEncoder.ResY = %d"), VideoConfig.Height);
	if (VideoConfig.Height == 720)
	{
		VideoConfig.Width = 1280;
		VideoConfig.Height = 720;
	}
	else if (VideoConfig.Height == 0 || VideoConfig.Height == 1080)
	{
		VideoConfig.Width = 1920;
		VideoConfig.Height = 1080;
	}
	else
	{
		UE_LOG(LogTemp, Fatal, TEXT("GameplayMediaEncoder.ResY can only have a value of 720 or 1080"));
		VideoConfig.Width = 1920;
		VideoConfig.Height = 1080;
	}

	// Specifying 0 will completely disable frame skipping (therefore encoding as many frames as possible)
	FParse::Value(FCommandLine::Get(), TEXT("GameplayMediaEncoder.FPS="), VideoConfig.Framerate);
	if (VideoConfig.Framerate == 0)
	{
		// Note : When disabling frame skipping, we lie to the encoder when initializing.
		// We still specify a framerate, but then feed frames without skipping
		VideoConfig.Framerate = 30;
		UE_LOG(LogTemp, Log, TEXT("Uncapping FPS"));
	}
	else
	{
		VideoConfig.Framerate = FMath::Clamp(VideoConfig.Framerate, (uint32)10, (uint32)60);
		UE_LOG(LogTemp, Log, TEXT("Capping FPS %u"), VideoConfig.Framerate);
	}

	VideoConfig.Bitrate = 20000000;
	FParse::Value(FCommandLine::Get(), TEXT("GameplayMediaEncoder.Bitrate="), VideoConfig.Bitrate);
	VideoConfig.Bitrate = FMath::Clamp(VideoConfig.Bitrate, (uint32)1000000, (uint32)20000000);

	Muxer = MakeUnique<FMP4Muxer>();
	if (!Muxer->Initialize(FilePath, VideoConfig, AudioConfig))
	{
		// Handle initialization failure
		Muxer.Reset();
		bIsInitialize = false;
	}

	bSuccess = bIsInitialize;
	AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnAsyncInitCompleted(bSuccess);
			}
		});

	return ;
}

bool AScreenRecordingManager::Start()
{
	if (bIsRecording)
	{
		return false;
	}

	if (!bIsInitialize)
	{
		return false;
	}

	Temp_Listener.Owner = this;
	bIsRecording = GME->RegisterListener(&Temp_Listener);
	//bIsRecording = GME->Start();

	return bIsRecording;
}

void AScreenRecordingManager::Stop()
{
	if (!bIsInitialize)
	{
		return;
	}

	GME->UnregisterListener(&Temp_Listener);
	bIsRecording = false;
	//GME->Stop();

	if (Muxer)
	{
		Muxer->Finalize();
		Muxer.Reset();
		GME->Shutdown();
		bIsInitialize = false;
	}
}

void AScreenRecordingManager::OnMediaSample(const AVEncoder::FMediaPacket& Sample)
{
	UE_LOG(LogTemp, Log, TEXT("Get Packet"));
	if (Muxer)
	{
		FScopeLock Lock(&MuxerCS);
		Muxer->AddPacket(Sample);
	}
}