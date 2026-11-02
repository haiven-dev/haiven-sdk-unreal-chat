#include "HaivenChatStarterGameMode.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HaivenChatStarterSettings.h"
#include "HaivenChatStarterSubsystem.h"

void AHaivenChatStarterGameMode::BeginPlay()
{
	Super::BeginPlay();
	UHaivenChatStarterSubsystem* Starter = GetGameInstance()->GetSubsystem<UHaivenChatStarterSubsystem>();
	if (!Starter) return;
	Starter->Show(GetWorld()->GetFirstPlayerController());
	if (GetDefault<UHaivenChatStarterSettings>()->bAutoConnect) Starter->Connect();
}
