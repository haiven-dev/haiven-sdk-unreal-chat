#include "HaivenChatStarterWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HaivenChatStarterSubsystem.h"

namespace
{
	UTextBlock* ButtonLabel(UWidgetTree* Tree, const FText& Text)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
		Label->SetText(Text);
		return Label;
	}
}

TSharedRef<SWidget> UHaivenChatStarterWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Root"));
		Root->SetWidthOverride(640.0f);
		Root->SetHeightOverride(420.0f);

		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
		Background->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.04f, 0.94f));
		Background->SetPadding(FMargin(16.0f));
		Root->AddChild(Background);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Background->SetContent(Column);

		UTextBlock* Heading = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Heading"));
		Heading->SetText(NSLOCTEXT("HaivenChatStarter", "Heading", "Haiven Chat Starter"));
		FSlateFontInfo HeadingFont = Heading->GetFont();
		HeadingFont.Size = 24;
		Heading->SetFont(HeadingFont);
		Column->AddChildToVerticalBox(Heading);

		Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConnectionStatus"));
		Status->SetText(NSLOCTEXT("HaivenChatStarter", "InitialStatus", "not started"));
		Column->AddChildToVerticalBox(Status);

		Log = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("MessageLog"));
		if (UVerticalBoxSlot* LogSlot = Column->AddChildToVerticalBox(Log))
		{
			LogSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LogSlot->SetPadding(FMargin(0.0f, 12.0f));
		}

		UHorizontalBox* Composer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Composer"));
		Column->AddChildToVerticalBox(Composer);

		Input = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("MessageInput"));
		Input->SetHintText(NSLOCTEXT("HaivenChatStarter", "InputHint", "Message"));
		Input->OnTextCommitted.AddDynamic(this, &UHaivenChatStarterWidget::HandleTextCommitted);
		if (UHorizontalBoxSlot* InputSlot = Composer->AddChildToHorizontalBox(Input))
		{
			InputSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		UButton* Send = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SendChatMessage"));
		Send->SetContent(ButtonLabel(WidgetTree, NSLOCTEXT("HaivenChatStarter", "Send", "Send Chat Message")));
		Send->OnClicked.AddDynamic(this, &UHaivenChatStarterWidget::HandleSendClicked);
		Composer->AddChildToHorizontalBox(Send);

		UButton* Reconnect = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Reconnect"));
		Reconnect->SetContent(ButtonLabel(WidgetTree, NSLOCTEXT("HaivenChatStarter", "Reconnect", "Reconnect")));
		Reconnect->OnClicked.AddDynamic(this, &UHaivenChatStarterWidget::HandleReconnectClicked);
		Composer->AddChildToHorizontalBox(Reconnect);

		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void UHaivenChatStarterWidget::SetConnectionStatus(const FText& Text)
{
	if (Status) Status->SetText(Text);
}

UTextBlock* UHaivenChatStarterWidget::MakeLine(const FText& Text)
{
	if (!Log) return nullptr;
	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
	Line->SetText(Text);
	Line->SetAutoWrapText(true);
	Log->AddChild(Line);
	Log->ScrollToEnd();
	return Line;
}

void UHaivenChatStarterWidget::AddLine(const FText& Text)
{
	MakeLine(Text);
}

void UHaivenChatStarterWidget::SetOutboxLine(const FString& ClientMessageId, const FText& Text)
{
	if (ReconcileOutboxLine(ClientMessageId, Text)) return;
	if (UTextBlock* Line = MakeLine(Text)) OutboxLines.Add(ClientMessageId, Line);
}

bool UHaivenChatStarterWidget::ReconcileOutboxLine(const FString& ClientMessageId, const FText& Text)
{
	if (const TObjectPtr<UTextBlock>* Existing = OutboxLines.Find(ClientMessageId))
	{
		(*Existing)->SetText(Text);
		return true;
	}
	return false;
}

void UHaivenChatStarterWidget::FocusInput()
{
	if (Input) Input->SetKeyboardFocus();
}

void UHaivenChatStarterWidget::HandleSendClicked()
{
	if (!Input || Input->GetText().IsEmptyOrWhitespace()) return;
	if (UHaivenChatStarterSubsystem* Starter = GetGameInstance()->GetSubsystem<UHaivenChatStarterSubsystem>())
	{
		Starter->Send(Input->GetText().ToString());
		Input->SetText(FText::GetEmpty());
	}
}

void UHaivenChatStarterWidget::HandleReconnectClicked()
{
	if (UHaivenChatStarterSubsystem* Starter = GetGameInstance()->GetSubsystem<UHaivenChatStarterSubsystem>())
	{
		Starter->Connect();
	}
}

void UHaivenChatStarterWidget::HandleTextCommitted(const FText& Text, const ETextCommit::Type Method)
{
	if (Method == ETextCommit::OnEnter) HandleSendClicked();
}
