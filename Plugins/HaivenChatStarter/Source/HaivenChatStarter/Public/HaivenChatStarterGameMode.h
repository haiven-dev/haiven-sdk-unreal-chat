#pragma once

#include "GameFramework/GameModeBase.h"
#include "HaivenChatStarterGameMode.generated.h"

UCLASS()
class HAIVENCHATSTARTER_API AHaivenChatStarterGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};
