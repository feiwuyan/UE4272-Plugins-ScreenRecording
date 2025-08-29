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

// ����һ����ͼ�ɰ󶨵�ί�У��������첽��ʼ����ɺ�֪ͨ��ͼ
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

	// ʵ��ִ�г�ʼ���߼��ĺ��������ں�̨�߳��е���
	void PerformAsyncInitialization();

	// ��ͼ�ɰ󶨵�ί�У��������첽��ʼ����ɺ�֪ͨ��ͼ
	UPROPERTY(BlueprintAssignable)
	FOnInitCompletedSignature OnInitCompleted;

	// ��̨��ʼ����ɺ󣬻ص� GameThread ����״̬�ĺ���
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
