#pragma once

#include "CoreMinimal.h"
#include "Blueprint/HaivenBlueprintTypes.h"
#include "HaivenChat.h"
#include "HaivenClientTypes.h"

class UHaivenChatStarterWidget;

namespace HaivenChatStarter
{
	HAIVENCHATSTARTER_API FText DescribeConnection(EHaivenRealtimeState State);
	HAIVENCHATSTARTER_API FText DescribeIncoming(const FHaivenChatMessageV0& Message);
	HAIVENCHATSTARTER_API FText DescribeTyping(const FHaivenChatTypingV0& Typing);
	HAIVENCHATSTARTER_API FText DescribeOutgoing(const FHaivenOutgoingChatMessage& Message);
	HAIVENCHATSTARTER_API bool ReconcileIncoming(
		UHaivenChatStarterWidget& Panel,
		const FHaivenChatMessageV0& Message,
		const TArray<FHaivenOutgoingChatMessage>& Outbox);
}
