// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SRGameplayMediaEncoder.h"
#include "AudioEncoder.h"
#include "VideoEncoder.h"
#include "MediaPacket.h"
#include "VideoEncoderInput.h"
#include "MP4Muxer.h" 

#include "ScreenRecordingManager.generated.h"

// 定义一个蓝图可绑定的委托，用于在异步初始化完成后通知蓝图
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInitCompletedSignature, bool, bSuccess);

class SRM_Listener : public IGameplayMediaEncoderListener
{
public:
	void OnMediaSample(const AVEncoder::FMediaPacket& Sample) override;
	AActor* Owner;
};

UCLASS()
class SCREENRECORDING_API AScreenRecordingManager : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AScreenRecordingManager();
	~AScreenRecordingManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	void OnMediaSample(const AVEncoder::FMediaPacket& Sample);

	// 实际执行初始化逻辑的函数，将在后台线程中调用
	void PerformAsyncInitialization();

	// 蓝图可绑定的委托，用于在异步初始化完成后通知蓝图
	UPROPERTY(BlueprintAssignable)
	FOnInitCompletedSignature OnInitCompleted;

	// 后台初始化完成后，回到 GameThread 更新状态的函数
    void OnAsyncInitCompleted(bool bSuccess);

	UFUNCTION(BlueprintCallable)
	void Initialize();
	UFUNCTION(BlueprintCallable)
	bool Start();
	UFUNCTION(BlueprintCallable)
	void Stop();

	FSRGameplayMediaEncoder* GME;
	SRM_Listener Temp_Listener;

	TUniquePtr<FMP4Muxer> Muxer;
	FCriticalSection MuxerCS;

	bool bIsRecording;
	bool bIsInitialize;
	bool AsyncLock;
};
