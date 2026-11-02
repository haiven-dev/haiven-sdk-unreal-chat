#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HaivenChatStarterSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Haiven Chat Starter"))
class HAIVENCHATSTARTER_API UHaivenChatStarterSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Connection")
	FString BaseUrl;

	UPROPERTY(Config, EditAnywhere, Category = "Connection")
	FString RealtimeUrl;

	UPROPERTY(Config, EditAnywhere, Category = "Connection")
	FString AppId;

	UPROPERTY(Config, EditAnywhere, Category = "Chat")
	FString ChannelId;

	UPROPERTY(Config, EditAnywhere, Category = "Connection")
	bool bAutoConnect = true;

	bool IsConfigured() const
	{
		return !BaseUrl.IsEmpty() && !RealtimeUrl.IsEmpty() && !AppId.IsEmpty() && !ChannelId.IsEmpty();
	}
};
