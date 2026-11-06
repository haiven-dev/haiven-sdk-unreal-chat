#include "HaivenChatStarterWidget.h"

#include "Runtime/Launch/Resources/Version.h"
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
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "HaivenChatStarterSubsystem.h"

namespace
{
	// One dark palette shared by every control, so the panel reads as a single theme.
	namespace Theme
	{
		const FLinearColor Panel(0.015f, 0.02f, 0.04f, 0.94f);
		const FLinearColor Field(0.03f, 0.04f, 0.075f, 1.0f);
		const FLinearColor Control(0.06f, 0.08f, 0.14f, 1.0f);
		const FLinearColor ControlHover(0.11f, 0.14f, 0.24f, 1.0f);
		const FLinearColor ControlPressed(0.025f, 0.035f, 0.07f, 1.0f);
		const FLinearColor Outline(0.16f, 0.2f, 0.32f, 1.0f);
		const FLinearColor Accent(0.25f, 0.5f, 1.0f, 1.0f);
		const FLinearColor Text(0.9f, 0.92f, 0.96f, 1.0f);
		const FLinearColor MutedText(0.55f, 0.6f, 0.7f, 1.0f);
		const FLinearColor ErrorText(0.95f, 0.55f, 0.4f, 1.0f);
		constexpr int32 TitleSize = 15;
		constexpr int32 BodySize = 11;
		constexpr float Radius = 6.0f;
		constexpr float OutlineWidth = 1.0f;

		void SetSize(UTextBlock* Block, const int32 Size)
		{
			FSlateFontInfo Font = Block->GetFont();
			Font.Size = Size;
			Block->SetFont(Font);
		}

		FSlateRoundedBoxBrush Box(const FLinearColor& Fill, const FLinearColor& Edge)
		{
			return FSlateRoundedBoxBrush(Fill, Radius, Edge, OutlineWidth);
		}

		FButtonStyle Button()
		{
			FButtonStyle Style;
			Style.SetNormal(Box(Control, Outline));
			Style.SetHovered(Box(ControlHover, Accent));
			Style.SetPressed(Box(ControlPressed, Accent));
			Style.SetDisabled(Box(Field, Outline));
			Style.SetNormalForeground(Text);
			Style.SetHoveredForeground(Text);
			Style.SetPressedForeground(Text);
			Style.SetDisabledForeground(MutedText);
			Style.SetNormalPadding(FMargin(14.0f, 6.0f));
			Style.SetPressedPadding(FMargin(14.0f, 7.0f, 14.0f, 5.0f));
			return Style;
		}

		FEditableTextBoxStyle Input(FEditableTextBoxStyle Style)
		{
			Style.SetBackgroundImageNormal(Box(Field, Outline));
			Style.SetBackgroundImageHovered(Box(Field, ControlHover));
			Style.SetBackgroundImageFocused(Box(Field, Accent));
			Style.SetBackgroundImageReadOnly(Box(Field, Outline));
			Style.SetBackgroundColor(FSlateColor(FLinearColor::White));
			Style.SetForegroundColor(FSlateColor(Text));
			Style.SetFocusedForegroundColor(FSlateColor(Text));
			Style.SetReadOnlyForegroundColor(FSlateColor(MutedText));
			Style.SetPadding(FMargin(10.0f, 6.0f));
			Style.TextStyle.Font.Size = BodySize;
			return Style;
		}
	}

	UTextBlock* ButtonLabel(UWidgetTree* Tree, const FText& Text)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
		Label->SetText(Text);
		Label->SetColorAndOpacity(FSlateColor(Theme::Text));
		Theme::SetSize(Label, Theme::BodySize);
		return Label;
	}

	UButton* ThemedButton(UWidgetTree* Tree, const FName Name, const FText& Text)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetStyle(Theme::Button());
		Button->SetContent(ButtonLabel(Tree, Text));
		return Button;
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
		Background->SetBrushColor(Theme::Panel);
		Background->SetPadding(FMargin(16.0f));
		Root->AddChild(Background);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Background->SetContent(Column);

		UTextBlock* Heading = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Heading"));
		Heading->SetText(NSLOCTEXT("HaivenChatStarter", "Heading", "Haiven Chat Starter"));
		Theme::SetSize(Heading, Theme::TitleSize);
		Column->AddChildToVerticalBox(Heading);

		Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConnectionStatus"));
		Status->SetText(NSLOCTEXT("HaivenChatStarter", "InitialStatus", "not started"));
		Status->SetColorAndOpacity(FSlateColor(Theme::MutedText));
		Theme::SetSize(Status, Theme::BodySize);
		Column->AddChildToVerticalBox(Status);

		Log = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("MessageLog"));
		if (UVerticalBoxSlot* LogSlot = Column->AddChildToVerticalBox(Log))
		{
			LogSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LogSlot->SetPadding(FMargin(0.0f, 12.0f));
		}

		Input = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("MessageInput"));
		Input->SetHintText(NSLOCTEXT("HaivenChatStarter", "InputHint", "Message"));
		// UE 5.6.1 has no SetWidgetStyle/GetWidgetStyle. Assign the public property there.
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
		Input->SetWidgetStyle(Theme::Input(Input->GetWidgetStyle()));
#else
		Input->WidgetStyle = Theme::Input(Input->WidgetStyle);
#endif
		Input->OnTextCommitted.AddDynamic(this, &UHaivenChatStarterWidget::HandleTextCommitted);
		Column->AddChildToVerticalBox(Input);

		UHorizontalBox* Composer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Composer"));
		if (UVerticalBoxSlot* ComposerSlot = Column->AddChildToVerticalBox(Composer))
		{
			ComposerSlot->SetHorizontalAlignment(HAlign_Right);
			ComposerSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		}

		UButton* Send = ThemedButton(WidgetTree, TEXT("SendChatMessage"), NSLOCTEXT("HaivenChatStarter", "Send", "Send Chat Message"));
		Send->OnClicked.AddDynamic(this, &UHaivenChatStarterWidget::HandleSendClicked);
		if (UHorizontalBoxSlot* SendSlot = Composer->AddChildToHorizontalBox(Send))
		{
			SendSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		}

		UButton* Reconnect = ThemedButton(WidgetTree, TEXT("Reconnect"), NSLOCTEXT("HaivenChatStarter", "Reconnect", "Reconnect"));
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

UTextBlock* UHaivenChatStarterWidget::MakeLine(const FText& Text, const bool bError)
{
	if (!Log) return nullptr;
	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
	Line->SetText(Text);
	Line->SetAutoWrapText(true);
	Theme::SetSize(Line, Theme::BodySize);
	Line->SetColorAndOpacity(FSlateColor(bError ? Theme::ErrorText : Theme::Text));
	Log->AddChild(Line);
	Log->ScrollToEnd();
	return Line;
}

void UHaivenChatStarterWidget::AddLine(const FText& Text)
{
	MakeLine(Text);
}

void UHaivenChatStarterWidget::AddErrorLine(const FText& Text)
{
	MakeLine(Text, true);
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
