#include "HaivenChatStarterPresentation.h"
#include "HaivenChatStarterWidget.h"

namespace HaivenChatStarter
{
	FText DescribeConnection(const EHaivenRealtimeState State)
	{
		switch (State)
		{
		case EHaivenRealtimeState::Disconnected:
			return NSLOCTEXT("HaivenChatStarter", "Disconnected", "offline — press Reconnect");
		case EHaivenRealtimeState::Connecting:
			return NSLOCTEXT("HaivenChatStarter", "Connecting", "connecting…");
		case EHaivenRealtimeState::Authenticating:
			return NSLOCTEXT("HaivenChatStarter", "Authenticating", "authenticating…");
		case EHaivenRealtimeState::Ready:
			return NSLOCTEXT("HaivenChatStarter", "Ready", "connected");
		}
		return NSLOCTEXT("HaivenChatStarter", "UnknownConnection", "unknown connection state");
	}

	FText DescribeIncoming(const FHaivenChatMessageV0& Message)
	{
		FString Name = TEXT("someone");
		if (Message.bHasAuthor)
		{
			Name = !Message.Author.DisplayName.IsEmpty() ? Message.Author.DisplayName : Message.Author.Username;
		}
		return FText::Format(NSLOCTEXT("HaivenChatStarter", "Incoming", "{0}: {1}"),
			FText::FromString(Name), FText::FromString(Message.bHasContent ? Message.Content : TEXT("[no content]")));
	}

	FText DescribeTyping(const FHaivenChatTypingV0& Typing)
	{
		const FString Who = Typing.bHasUserId ? Typing.UserId : TEXT("someone");
		return FText::Format(
			Typing.IsTyping
				? NSLOCTEXT("HaivenChatStarter", "Typing", "{0} is typing…")
				: NSLOCTEXT("HaivenChatStarter", "StoppedTyping", "{0} stopped typing"),
			FText::FromString(Who));
	}

	FText DescribeOutgoing(const FHaivenOutgoingChatMessage& Message)
	{
		const FText Content = FText::FromString(Message.Content);
		switch (Message.State)
		{
		case EHaivenOutgoingChatState::Sending:
			return FText::Format(NSLOCTEXT("HaivenChatStarter", "Sending", "you (sending): {0}"), Content);
		case EHaivenOutgoingChatState::Unconfirmed:
			return FText::Format(NSLOCTEXT("HaivenChatStarter", "Unconfirmed", "you (waiting to resend): {0}"), Content);
		case EHaivenOutgoingChatState::Accepted:
			return FText::Format(NSLOCTEXT("HaivenChatStarter", "Accepted", "you: {0}"), Content);
		case EHaivenOutgoingChatState::Held:
			return FText::Format(NSLOCTEXT("HaivenChatStarter", "Held", "you (held: {1}): {0}"),
				Content, FText::FromString(Message.ModerationState));
		case EHaivenOutgoingChatState::Removed:
			return FText::Format(NSLOCTEXT("HaivenChatStarter", "Removed", "you (not shown: {1}): {0}"),
				Content, FText::FromString(Message.ModerationState));
		case EHaivenOutgoingChatState::Refused:
			return FText::Format(NSLOCTEXT("HaivenChatStarter", "Refused", "you (refused: {1}): {0}"),
				Content, FText::FromString(Message.Error));
		}
		return Content;
	}

	bool ReconcileIncoming(
		UHaivenChatStarterWidget& Panel,
		const FHaivenChatMessageV0& Message,
		const TArray<FHaivenOutgoingChatMessage>& Outbox)
	{
		for (const FHaivenOutgoingChatMessage& Outgoing : Outbox)
		{
			const bool bMatchesClientMessage = Message.bHasClientMessageId
				&& Outgoing.ClientMessageId == Message.ClientMessageId;
			const bool bMatchesServerMessage = Outgoing.bHasMessageId
				&& Outgoing.MessageId == Message.Id;
			if (bMatchesClientMessage || bMatchesServerMessage)
			{
				return Panel.ReconcileOutboxLine(Outgoing.ClientMessageId, DescribeIncoming(Message));
			}
		}
		return false;
	}
}
