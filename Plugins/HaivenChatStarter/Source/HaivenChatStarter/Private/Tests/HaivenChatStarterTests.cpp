#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Blueprint/HaivenBlueprintContracts.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HaivenChatStarterSubsystem.h"
#include "HaivenChatStarterPresentation.h"
#include "HaivenChatStarterSettings.h"
#include "HaivenChatStarterWidget.h"
#include "HaivenFrames.h"
#include "HaivenSubsystem.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
	FString JsonFor(const TSharedPtr<FJsonObject>& Object)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
		return Json;
	}

	EHaivenOutgoingChatState OutboxState(const FString& Name)
	{
		if (Name == TEXT("unconfirmed")) return EHaivenOutgoingChatState::Unconfirmed;
		if (Name == TEXT("accepted")) return EHaivenOutgoingChatState::Accepted;
		if (Name == TEXT("held")) return EHaivenOutgoingChatState::Held;
		if (Name == TEXT("removed")) return EHaivenOutgoingChatState::Removed;
		if (Name == TEXT("refused")) return EHaivenOutgoingChatState::Refused;
		return EHaivenOutgoingChatState::Sending;
	}

	bool Authenticate(Haiven::FSessionCore& Session)
	{
		Session.Opened();
		Haiven::FServerFrame Frame;
		const std::string Json = R"({"type":"auth.success","requestId":"","success":true})";
		if (!Haiven::ParseServerFrame(Json, Frame)) return false;
		Session.Receive(Frame, Json);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenChatStarterFixtureTest,
	"Haiven.ChatStarter.Fixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenChatStarterFixtureTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("HaivenChatStarter"));
	if (!TestTrue(TEXT("starter plugin is registered"), Plugin.IsValid())) return false;
	const FString FixturePath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/Fixtures/chat-session.json"));
	FString FixtureText;
	if (!TestTrue(TEXT("fixture is readable"), FFileHelper::LoadFileToString(FixtureText, *FixturePath))) return false;

	TSharedPtr<FJsonObject> Fixture;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FixtureText);
	if (!TestTrue(TEXT("fixture is JSON"), FJsonSerializer::Deserialize(Reader, Fixture) && Fixture.IsValid())) return false;

	FHaivenChatMessageV0 Message;
	FHaivenChatTypingV0 Typing;
	FString Error;
	TestTrue(TEXT("typed chat.message parser accepts the fixture"),
		UHaivenBlueprintContracts::ParseChatMessageV0(JsonFor(Fixture->GetObjectField(TEXT("message"))), Message, Error));
	TestEqual(TEXT("typed incoming event is rendered"),
		HaivenChatStarter::DescribeIncoming(Message).ToString(), FString(TEXT("Mina: hello from the fixture")));

	Error.Reset();
	TestTrue(TEXT("typed chat.typing parser accepts the fixture"),
		UHaivenBlueprintContracts::ParseChatTypingV0(JsonFor(Fixture->GetObjectField(TEXT("typing"))), Typing, Error));
	TestEqual(TEXT("typed typing event is rendered"),
		HaivenChatStarter::DescribeTyping(Typing).ToString(), FString(TEXT("user-7 is typing…")));

	for (const TSharedPtr<FJsonValue>& Value : Fixture->GetArrayField(TEXT("outbox")))
	{
		const TSharedPtr<FJsonObject> Row = Value->AsObject();
		FHaivenOutgoingChatMessage Outgoing;
		Outgoing.ClientMessageId = TEXT("fixture-client-id");
		Outgoing.ChannelId = TEXT("general");
		Outgoing.Content = TEXT("fixture send");
		Outgoing.State = OutboxState(Row->GetStringField(TEXT("state")));
		Row->TryGetStringField(TEXT("moderationState"), Outgoing.ModerationState);
		Row->TryGetStringField(TEXT("error"), Outgoing.Error);
		TestEqual(*FString::Printf(TEXT("outbox state %s"), *Row->GetStringField(TEXT("state"))),
			HaivenChatStarter::DescribeOutgoing(Outgoing).ToString(), Row->GetStringField(TEXT("expected")));
	}

	TestEqual(TEXT("disconnect explains recovery"),
		HaivenChatStarter::DescribeConnection(EHaivenRealtimeState::Disconnected).ToString(),
		FString(TEXT("offline — press Reconnect")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenChatStarterPanelTest,
	"Haiven.ChatStarter.Panel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenChatStarterPanelTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("editor world"), World)) return false;
	UHaivenChatStarterWidget* Panel = CreateWidget<UHaivenChatStarterWidget>(World, UHaivenChatStarterWidget::StaticClass());
	if (!TestNotNull(TEXT("starter panel"), Panel)) return false;
	Panel->TakeWidget();

	UScrollBox* Log = Panel->WidgetTree ? Cast<UScrollBox>(Panel->WidgetTree->FindWidget(TEXT("MessageLog"))) : nullptr;
	if (!TestNotNull(TEXT("panel has a message log"), Log)) return false;
	TestNotNull(TEXT("panel has typed input"), Panel->WidgetTree->FindWidget(TEXT("MessageInput")));
	TestNotNull(TEXT("panel has Send Chat Message"), Panel->WidgetTree->FindWidget(TEXT("SendChatMessage")));
	TestNotNull(TEXT("panel has Reconnect"), Panel->WidgetTree->FindWidget(TEXT("Reconnect")));

	Panel->AddLine(FText::FromString(TEXT("connected")));
	Panel->SetOutboxLine(TEXT("one"), FText::FromString(TEXT("you (sending): hi")));
	Panel->SetOutboxLine(TEXT("one"), FText::FromString(TEXT("you: hi")));
	TestEqual(TEXT("outbox changes replace one keyed line"), Log->GetChildrenCount(), 2);

	Panel->SetOutboxLine(TEXT("held"), FText::FromString(TEXT("you (held for review): visible later")));
	TestTrue(TEXT("visible correlated event reconciles the held row"), Panel->ReconcileOutboxLine(
		TEXT("held"), FText::FromString(TEXT("Mina: visible later"))));
	TestEqual(TEXT("held-then-visible leaves one published row"), Log->GetChildrenCount(), 3);
	const UTextBlock* Published = Cast<UTextBlock>(Log->GetChildAt(2));
	TestNotNull(TEXT("reconciled row remains a text line"), Published);
	if (Published)
	{
		TestEqual(TEXT("reconciled row shows the visible message"),
			Published->GetText().ToString(), FString(TEXT("Mina: visible later")));
	}

	FHaivenOutgoingChatMessage Held;
	Held.ClientMessageId = TEXT("held-by-server-id");
	Held.ChannelId = TEXT("general");
	Held.Content = TEXT("released without client id");
	Held.State = EHaivenOutgoingChatState::Held;
	Held.bHasMessageId = true;
	Held.MessageId = TEXT("server-message-id");
	Panel->SetOutboxLine(Held.ClientMessageId, HaivenChatStarter::DescribeOutgoing(Held));

	FHaivenChatMessageV0 Released;
	Released.ChannelId = Held.ChannelId;
	Released.Id = Held.MessageId;
	Released.bHasAuthor = true;
	Released.Author.DisplayName = TEXT("Mina");
	Released.bHasContent = true;
	Released.Content = Held.Content;
	TArray<FHaivenOutgoingChatMessage> Outbox;
	Outbox.Add(Held);
	TestTrue(TEXT("visible event without client id reconciles by server message id"),
		HaivenChatStarter::ReconcileIncoming(*Panel, Released, Outbox));
	TestEqual(TEXT("server-id reconciliation does not append a duplicate"), Log->GetChildrenCount(), 4);
	const UTextBlock* ReleasedLine = Cast<UTextBlock>(Log->GetChildAt(3));
	TestNotNull(TEXT("server-id reconciled row remains a text line"), ReleasedLine);
	if (ReleasedLine)
	{
		TestEqual(TEXT("server-id reconciled row shows the released message"),
			ReleasedLine->GetText().ToString(), FString(TEXT("Mina: released without client id")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenChatStarterSettingsTest,
	"Haiven.ChatStarter.Settings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenChatStarterSettingsTest::RunTest(const FString& Parameters)
{
	UHaivenChatStarterSettings* Settings = NewObject<UHaivenChatStarterSettings>();
	TestFalse(TEXT("blank project settings are refused"), Settings->IsConfigured());
	Settings->BaseUrl = TEXT("https://api.example.test/v0");
	Settings->RealtimeUrl = TEXT("wss://api.example.test/v0/ws");
	Settings->AppId = TEXT("starter");
	Settings->ChannelId = TEXT("general");
	TestTrue(TEXT("all public connection values configure the starter"), Settings->IsConfigured());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenChatStarterReconnectTest,
	"Haiven.ChatStarter.ReconnectRetainsOutbox",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenChatStarterReconnectTest::RunTest(const FString& Parameters)
{
	// Drive the starter's own Connect() twice with one unchanged demo bearer. Each
	// Connect() starts a real transport to a closed loopback port; the test then
	// detaches it and attaches a session it controls, so the frames are observable.
	UHaivenChatStarterSettings* Settings = GetMutableDefault<UHaivenChatStarterSettings>();
	const FString SavedBaseUrl = Settings->BaseUrl;
	const FString SavedRealtimeUrl = Settings->RealtimeUrl;
	const FString SavedAppId = Settings->AppId;
	const FString SavedChannelId = Settings->ChannelId;
	const FString SavedToken = FPlatformMisc::GetEnvironmentVariable(TEXT("HAIVEN_DEMO_ACCESS_TOKEN"));
	ON_SCOPE_EXIT
	{
		Settings->BaseUrl = SavedBaseUrl;
		Settings->RealtimeUrl = SavedRealtimeUrl;
		Settings->AppId = SavedAppId;
		Settings->ChannelId = SavedChannelId;
		FPlatformMisc::SetEnvironmentVar(TEXT("HAIVEN_DEMO_ACCESS_TOKEN"), *SavedToken);
	};
	Settings->BaseUrl = TEXT("http://127.0.0.1:9");
	Settings->RealtimeUrl = TEXT("ws://127.0.0.1:9");
	Settings->AppId = TEXT("starter-test");
	Settings->ChannelId = TEXT("general");
	FPlatformMisc::SetEnvironmentVar(TEXT("HAIVEN_DEMO_ACCESS_TOKEN"), TEXT("demo-bearer"));

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHaivenSubsystem* HaivenSubsystem = NewObject<UHaivenSubsystem>(GameInstance);
	UHaivenChatStarterSubsystem* Starter = NewObject<UHaivenChatStarterSubsystem>(GameInstance);
	Starter->Haiven = HaivenSubsystem;
	auto AttachControlledSession = [HaivenSubsystem](
		std::vector<Haiven::FClientFrame>& Writes, int32& Ids, const std::string& Prefix)
	{
		HaivenSubsystem->DisconnectRealtime();
		HaivenSubsystem->RealtimeSession = MakeUnique<Haiven::FSessionCore>(
			[&Writes](const Haiven::FClientFrame& Frame) { Writes.push_back(Frame); },
			[] { return std::string("demo-bearer"); },
			[&Ids, Prefix] { return Prefix + std::to_string(++Ids); });
		HaivenSubsystem->RealtimeSession->OnClosed = [HaivenSubsystem](int, const std::string&)
		{
			if (HaivenSubsystem->Chat) HaivenSubsystem->Chat->HandleClosed();
		};
		HaivenSubsystem->Chat->Attach(*HaivenSubsystem->RealtimeSession);
	};

	Starter->Connect();
	TestTrue(TEXT("the first Connect() applies the demo bearer"), Starter->AppliedAccessToken == TEXT("demo-bearer"));
	std::vector<Haiven::FClientFrame> FirstWrites;
	int32 FirstIds = 0;
	AttachControlledSession(FirstWrites, FirstIds, "first-");
	TestTrue(TEXT("the first demo session authenticates"), Authenticate(*HaivenSubsystem->RealtimeSession));
	Starter->Send(TEXT("survive reconnect"));
	const TArray<FHaivenOutgoingChatMessage> FirstOutbox = HaivenSubsystem->GetChatOutbox();
	if (!TestEqual(TEXT("the demo send enters the outbox"), FirstOutbox.Num(), 1)) return false;
	const FString ClientMessageId = FirstOutbox[0].ClientMessageId;
	HaivenSubsystem->RealtimeSession->Closed(1006, "network");
	TestTrue(TEXT("the closed send waits for resend"),
		HaivenSubsystem->GetChatOutbox()[0].State == EHaivenOutgoingChatState::Unconfirmed);

	Starter->Connect();
	const TArray<FHaivenOutgoingChatMessage> Retained = HaivenSubsystem->GetChatOutbox();
	TestTrue(TEXT("Connect() with the unchanged demo bearer retains the queued message"),
		Retained.Num() == 1 && Retained[0].ClientMessageId == ClientMessageId);

	std::vector<Haiven::FClientFrame> SecondWrites;
	int32 SecondIds = 0;
	AttachControlledSession(SecondWrites, SecondIds, "second-");
	TestTrue(TEXT("the replacement demo session authenticates"), Authenticate(*HaivenSubsystem->RealtimeSession));
	const auto Resent = std::find_if(SecondWrites.begin(), SecondWrites.end(), [](const Haiven::FClientFrame& Frame)
	{
		return Frame.Type == "chat.send";
	});
	TestTrue(TEXT("the next session resends the same client message id"),
		Resent != SecondWrites.end() && Resent->PayloadJson.has_value()
		&& Resent->PayloadJson->find("\"clientMessageId\":\"" + std::string(TCHAR_TO_UTF8(*ClientMessageId)) + "\"")
			!= std::string::npos);
	HaivenSubsystem->DisconnectRealtime();
	HaivenSubsystem->ReleaseRetiredRealtimeSessions();
	return true;
}

#endif
