#pragma once

#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "HaivenChatStarterWidget.generated.h"

class UEditableTextBox;
class UScrollBox;
class UTextBlock;

UCLASS()
class HAIVENCHATSTARTER_API UHaivenChatStarterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetConnectionStatus(const FText& Text);
	void AddLine(const FText& Text);
	void SetOutboxLine(const FString& ClientMessageId, const FText& Text);
	bool ReconcileOutboxLine(const FString& ClientMessageId, const FText& Text);
	void FocusInput();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION()
	void HandleSendClicked();

	UFUNCTION()
	void HandleReconnectClicked();

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type Method);

	UTextBlock* MakeLine(const FText& Text);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Status;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> Log;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> Input;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTextBlock>> OutboxLines;
};
