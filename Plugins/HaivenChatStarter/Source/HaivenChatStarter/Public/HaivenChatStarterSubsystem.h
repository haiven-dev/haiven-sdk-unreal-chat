#pragma once

#include "CoreMinimal.h"
#include "Blueprint/HaivenBlueprintTypes.h"
#include "HaivenChat.h"
#include "HaivenClientTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HaivenChatStarterSubsystem.generated.h"

class APlayerController;
class UHaivenChatStarterWidget;
class UHaivenRealtimeEvents;
class UHaivenSubsystem;

UCLASS()
class HAIVENCHATSTARTER_API UHaivenChatStarterSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void Show(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Haiven|Starter")
	void Connect();

	UFUNCTION(BlueprintCallable, Category = "Haiven|Starter")
	void Send(const FString& Content);

private:
	friend class FHaivenChatStarterReconnectTest;

	void ApplyDemoCredential(const FString& Token, FDateTime ExpiresAtUtc);
	void Note(const FText& Text) const;
	void HandleConnectComplete(bool bSucceeded, const FString& Error);

	UFUNCTION()
	void HandleRealtimeState(EHaivenRealtimeState State);

	UFUNCTION()
	void HandleRealtimeError(const FString& Error);

	UFUNCTION()
	void HandleRealtimeClosed(int32 Code, const FString& Reason);

	UFUNCTION()
	void HandleChatMessage(const FHaivenChatMessageV0& Message);

	UFUNCTION()
	void HandleChatTyping(const FHaivenChatTypingV0& Typing);

	UFUNCTION()
	void HandleOutgoingChat(const FHaivenOutgoingChatMessage& Message);

	UPROPERTY(Transient)
	TObjectPtr<UHaivenSubsystem> Haiven;

	UPROPERTY(Transient)
	TObjectPtr<UHaivenRealtimeEvents> Events;

	UPROPERTY(Transient)
	TObjectPtr<UHaivenChatStarterWidget> Panel;

	FString ChannelId;
	FString AppliedAccessToken;
};
