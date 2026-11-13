#include "HaivenChatStarterSubsystem.h"

#include "Blueprint/HaivenBlueprintRealtime.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HaivenChatStarterPresentation.h"
#include "HaivenChatStarterSettings.h"
#include "HaivenChatStarterWidget.h"
#include "HaivenSubsystem.h"

void UHaivenChatStarterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Haiven = Collection.InitializeDependency<UHaivenSubsystem>();
	Events = Collection.InitializeDependency<UHaivenRealtimeEvents>();
	if (!Haiven || !Events) return;

	Haiven->OnRealtimeStateChanged.AddDynamic(this, &UHaivenChatStarterSubsystem::HandleRealtimeState);
	Haiven->OnRealtimeError.AddDynamic(this, &UHaivenChatStarterSubsystem::HandleRealtimeError);
	Haiven->OnRealtimeClosed.AddDynamic(this, &UHaivenChatStarterSubsystem::HandleRealtimeClosed);
	Haiven->OnOutgoingChatChanged.AddDynamic(this, &UHaivenChatStarterSubsystem::HandleOutgoingChat);
	Events->OnChatMessage.AddDynamic(this, &UHaivenChatStarterSubsystem::HandleChatMessage);
	Events->OnChatTyping.AddDynamic(this, &UHaivenChatStarterSubsystem::HandleChatTyping);
}

void UHaivenChatStarterSubsystem::Deinitialize()
{
	if (Events)
	{
		Events->OnChatMessage.RemoveAll(this);
		Events->OnChatTyping.RemoveAll(this);
	}
	if (Haiven)
	{
		Haiven->OnRealtimeStateChanged.RemoveAll(this);
		Haiven->OnRealtimeError.RemoveAll(this);
		Haiven->OnRealtimeClosed.RemoveAll(this);
		Haiven->OnOutgoingChatChanged.RemoveAll(this);
	}
	Panel = nullptr;
	Super::Deinitialize();
}

void UHaivenChatStarterSubsystem::Show(APlayerController* PlayerController)
{
	if (!PlayerController || Panel) return;
	Panel = CreateWidget<UHaivenChatStarterWidget>(PlayerController, UHaivenChatStarterWidget::StaticClass());
	if (!Panel) return;
	Panel->AddToViewport(100);
	Panel->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	// Do not call SetPositionInViewport here: it resets the slot's anchors to (0,0),
	// which centres the panel on the viewport's top-left corner, mostly off-screen.
	// Zero offsets with a point anchor auto-size the panel to its 640x420 SizeBox.
	Panel->SetAnchorsInViewport(FAnchors(0.5f));
	Panel->SetConnectionStatus(HaivenChatStarter::DescribeConnection(
		Haiven ? Haiven->GetRealtimeState() : EHaivenRealtimeState::Disconnected));
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(Panel->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(Mode);
	PlayerController->SetShowMouseCursor(true);
	Panel->FocusInput();
}

void UHaivenChatStarterSubsystem::Connect()
{
	if (!Haiven)
	{
		NoteError(NSLOCTEXT("HaivenChatStarter", "NoSubsystem", "connection failed: Haiven subsystem is unavailable"));
		return;
	}
	const UHaivenChatStarterSettings* Settings = GetDefault<UHaivenChatStarterSettings>();
	if (!Settings->IsConfigured())
	{
		NoteError(NSLOCTEXT("HaivenChatStarter", "NotConfigured", "connection failed: configure BaseUrl, RealtimeUrl, AppId, and ChannelId"));
		return;
	}
	const FString Token = FPlatformMisc::GetEnvironmentVariable(TEXT("HAIVEN_DEMO_ACCESS_TOKEN"));
	if (Token.IsEmpty())
	{
		NoteError(NSLOCTEXT("HaivenChatStarter", "NoToken", "connection failed: HAIVEN_DEMO_ACCESS_TOKEN is not set"));
		return;
	}

	if (Haiven->GetRealtimeState() != EHaivenRealtimeState::Disconnected)
	{
		Haiven->DisconnectRealtime();
	}
	ChannelId = Settings->ChannelId;
	FHaivenClientOptions Options;
	Options.BaseUrl = Settings->BaseUrl;
	Options.RealtimeUrl = Settings->RealtimeUrl;
	Options.AppId = Settings->AppId;
	Haiven->Configure(Options);
	ApplyDemoCredential(Token, FDateTime::UtcNow() + FTimespan::FromHours(1.0));
	Haiven->ConnectRealtime(FHaivenConnectComplete::CreateUObject(this, &UHaivenChatStarterSubsystem::HandleConnectComplete));
}

void UHaivenChatStarterSubsystem::ApplyDemoCredential(const FString& Token, FDateTime ExpiresAtUtc)
{
	if (!Haiven) return;
	if (AppliedAccessToken.IsEmpty() || AppliedAccessToken != Token)
	{
		// A changed environment credential may belong to another account, so the
		// full boundary is intentional in that case.
		Haiven->SetAccessToken(Token, ExpiresAtUtc);
		AppliedAccessToken = Token;
		return;
	}
	// Reconnect with the same demo bearer must retain the unconfirmed outbox.
	Haiven->RefreshAccessToken(Token, ExpiresAtUtc);
}

void UHaivenChatStarterSubsystem::Send(const FString& Content)
{
	if (!Haiven)
	{
		NoteError(NSLOCTEXT("HaivenChatStarter", "SendNoSubsystem", "send failed: Haiven subsystem is unavailable"));
		return;
	}
	FHaivenOutgoingChatMessage Message;
	FString Error;
	if (!Haiven->SendChatMessage(ChannelId, Content, false, FString(), Message, Error))
	{
		NoteError(FText::Format(NSLOCTEXT("HaivenChatStarter", "SendFailed", "send failed: {0}"), FText::FromString(Error)));
		return;
	}
	HandleOutgoingChat(Message);
}

void UHaivenChatStarterSubsystem::HandleConnectComplete(const bool bSucceeded, const FString& Error)
{
	if (!bSucceeded)
	{
		NoteError(FText::Format(NSLOCTEXT("HaivenChatStarter", "ConnectFailed", "connection refused: {0}"),
			FText::FromString(Error.IsEmpty() ? TEXT("unknown error") : Error)));
	}
}

void UHaivenChatStarterSubsystem::HandleRealtimeState(const EHaivenRealtimeState State)
{
	if (Panel) Panel->SetConnectionStatus(HaivenChatStarter::DescribeConnection(State));
	if (State == EHaivenRealtimeState::Ready)
	{
		if (Haiven && !ChannelId.IsEmpty())
		{
			UHaivenChatChannelActiveAsyncAction* Action =
				UHaivenChatChannelActiveAsyncAction::RealtimeChatChannelActive(this, Haiven, ChannelId);
			if (Action) Action->Activate();
		}
		Note(NSLOCTEXT("HaivenChatStarter", "Connected", "connected — queued messages are being sent"));
	}
}

void UHaivenChatStarterSubsystem::HandleRealtimeError(const FString& Error)
{
	NoteError(FText::Format(NSLOCTEXT("HaivenChatStarter", "SocketError", "realtime failure: {0}"), FText::FromString(Error)));
}

void UHaivenChatStarterSubsystem::HandleRealtimeClosed(const int32 Code, const FString& Reason)
{
	NoteError(FText::Format(NSLOCTEXT("HaivenChatStarter", "SocketClosed", "disconnected ({0}): {1}; unconfirmed sends stay queued"),
		FText::AsNumber(Code), FText::FromString(Reason)));
}

void UHaivenChatStarterSubsystem::HandleChatMessage(const FHaivenChatMessageV0& Message)
{
	if (Message.ChannelId != ChannelId) return;
	if (Panel && Haiven && HaivenChatStarter::ReconcileIncoming(*Panel, Message, Haiven->GetChatOutbox())) return;
	Note(HaivenChatStarter::DescribeIncoming(Message));
}

void UHaivenChatStarterSubsystem::HandleChatTyping(const FHaivenChatTypingV0& Typing)
{
	if (Typing.ChannelId == ChannelId) Note(HaivenChatStarter::DescribeTyping(Typing));
}

void UHaivenChatStarterSubsystem::HandleOutgoingChat(const FHaivenOutgoingChatMessage& Message)
{
	if (Panel && Message.ChannelId == ChannelId)
	{
		Panel->SetOutboxLine(Message.ClientMessageId, HaivenChatStarter::DescribeOutgoing(Message));
	}
}

void UHaivenChatStarterSubsystem::Note(const FText& Text) const
{
	if (Panel) Panel->AddLine(Text);
}

void UHaivenChatStarterSubsystem::NoteError(const FText& Text) const
{
	if (Panel) Panel->AddErrorLine(Text);
}
