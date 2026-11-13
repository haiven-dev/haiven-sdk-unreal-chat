// The engine face of the game login handoff. Every rule lives in the C++ SDK's
// `Haiven::Handoff::FCore`, which `sdks/cpp/test/handoff_test.cpp` checks under an
// ordinary compiler; this file only converts engine strings, byte views and
// `FDateTime` to and from it. `FDateTime` ticks are the core's `FTicks`, so time
// converts exactly.
#include "HaivenHandoff.h"

#include <string>
#include <vector>

// Named rather than anonymous: a unity build joins this file with the plugin's
// others, and an anonymous helper of the same name there would collide.
namespace HaivenHandoffDetail
{
	std::string ToUtf8(const FString& Value)
	{
		FTCHARToUTF8 Converted(*Value);
		return std::string(Converted.Get(), Converted.Length());
	}

	FString FromUtf8(const std::string& Value)
	{
		return FString(UTF8_TO_TCHAR(Value.c_str()));
	}
}

namespace
{
	Haiven::Handoff::FTicks Ticks(const FDateTime& Value)
	{
		return Haiven::Handoff::FTicks(Value.GetTicks());
	}

	std::vector<std::uint8_t> Bytes(TConstArrayView<uint8> View)
	{
		return std::vector<std::uint8_t>(View.GetData(), View.GetData() + View.Num());
	}

	/// The entropy copies are the state and verifier in waiting; clear them.
	void Wipe(std::vector<std::uint8_t>& Value)
	{
		volatile std::uint8_t* Data = Value.data();
		for (std::size_t Index = 0; Index < Value.size(); ++Index) Data[Index] = 0;
		Value.clear();
	}

	Haiven::FHandoffCredential ToEngine(const Haiven::Handoff::FCredential& Credential)
	{
		Haiven::FHandoffCredential Engine;
		Engine.AccessToken = HaivenHandoffDetail::FromUtf8(Credential.AccessToken);
		Engine.TokenType = HaivenHandoffDetail::FromUtf8(Credential.TokenType);
		Engine.bHasExpiry = Credential.bHasExpiry;
		Engine.ExpiresAt = Credential.bHasExpiry ? FDateTime(Credential.ExpiresAt.count()) : FDateTime();
		return Engine;
	}

	Haiven::Handoff::FCredential ToCore(const Haiven::FHandoffCredential& Credential)
	{
		Haiven::Handoff::FCredential Core;
		Core.AccessToken = HaivenHandoffDetail::ToUtf8(Credential.AccessToken);
		Core.TokenType = HaivenHandoffDetail::ToUtf8(Credential.TokenType);
		Core.bHasExpiry = Credential.bHasExpiry;
		Core.ExpiresAt = Credential.bHasExpiry ? Ticks(Credential.ExpiresAt) : Haiven::Handoff::FTicks::zero();
		return Core;
	}

	Haiven::Handoff::FConfig ToCore(const Haiven::FHandoffConfig& Config)
	{
		Haiven::Handoff::FConfig Core;
		Core.EntryUrl = HaivenHandoffDetail::ToUtf8(Config.EntryUrl);
		Core.ExchangeUrl = HaivenHandoffDetail::ToUtf8(Config.ExchangeUrl);
		Core.IntegrationId = HaivenHandoffDetail::ToUtf8(Config.IntegrationId);
		Core.RedirectUrl = HaivenHandoffDetail::ToUtf8(Config.RedirectUrl);
		Core.InstructionsId = HaivenHandoffDetail::ToUtf8(Config.InstructionsId);
		Core.ScreenHint = HaivenHandoffDetail::ToUtf8(Config.ScreenHint);
		Core.TransactionLifetime = Haiven::Handoff::FTicks(Config.TransactionLifetime.GetTicks());
		return Core;
	}
}

namespace Haiven
{
	FString FHandoffExchange::ToFormBody() const
	{
		Handoff::FExchange Exchange;
		Exchange.Code = HaivenHandoffDetail::ToUtf8(Code);
		Exchange.CodeVerifier = HaivenHandoffDetail::ToUtf8(CodeVerifier);
		Exchange.IntegrationId = HaivenHandoffDetail::ToUtf8(IntegrationId);
		Exchange.RedirectUrl = HaivenHandoffDetail::ToUtf8(RedirectUrl);
		return HaivenHandoffDetail::FromUtf8(Exchange.ToFormBody());
	}

	bool FHandoffCore::Begin(
		const FHandoffConfig& Config,
		TConstArrayView<uint8> StateEntropy,
		TConstArrayView<uint8> VerifierEntropy,
		const FDateTime& Now,
		FString& OutAuthorizationUrl,
		FString& OutState,
		FString& OutError)
	{
		std::vector<std::uint8_t> State = Bytes(StateEntropy);
		std::vector<std::uint8_t> Verifier = Bytes(VerifierEntropy);
		std::string Url;
		std::string StateText;
		std::string Error;
		const bool bBegan = Core.Begin(ToCore(Config), State, Verifier, Ticks(Now), Url, StateText, Error);
		Wipe(State);
		Wipe(Verifier);
		if (!bBegan)
		{
			OutError = HaivenHandoffDetail::FromUtf8(Error);
			return false;
		}
		OutAuthorizationUrl = HaivenHandoffDetail::FromUtf8(Url);
		OutState = HaivenHandoffDetail::FromUtf8(StateText);
		return true;
	}

	bool FHandoffCore::ConsumeCallback(
		const FString& CallbackUrl,
		const FDateTime& Now,
		FHandoffExchange& OutExchange,
		FString& OutError)
	{
		Handoff::FExchange Exchange;
		std::string Error;
		if (!Core.ConsumeCallback(HaivenHandoffDetail::ToUtf8(CallbackUrl), Ticks(Now), Exchange, Error))
		{
			OutError = HaivenHandoffDetail::FromUtf8(Error);
			return false;
		}
		OutExchange.ExchangeUrl = HaivenHandoffDetail::FromUtf8(Exchange.ExchangeUrl);
		OutExchange.Code = HaivenHandoffDetail::FromUtf8(Exchange.Code);
		OutExchange.CodeVerifier = HaivenHandoffDetail::FromUtf8(Exchange.CodeVerifier);
		OutExchange.IntegrationId = HaivenHandoffDetail::FromUtf8(Exchange.IntegrationId);
		OutExchange.RedirectUrl = HaivenHandoffDetail::FromUtf8(Exchange.RedirectUrl);
		return true;
	}

	void FHandoffCore::Cancel()
	{
		Core.Cancel();
	}

	bool FHandoffCore::HasPendingTransaction() const
	{
		return Core.HasPendingTransaction();
	}

	bool FHandoffCore::StoreExchangeResponse(
		const FString& Json,
		const FDateTime& Now,
		FHandoffCredential& OutCredential,
		FString& OutError)
	{
		Handoff::FCredential Credential;
		std::string Error;
		if (!Core.StoreExchangeResponse(HaivenHandoffDetail::ToUtf8(Json), Ticks(Now), Credential, Error))
		{
			OutError = HaivenHandoffDetail::FromUtf8(Error);
			return false;
		}
		OutCredential = ToEngine(Credential);
		return true;
	}

	bool FHandoffCore::GetAccessToken(const FDateTime& Now, FString& OutAccessToken) const
	{
		std::string Token;
		if (!Core.GetAccessToken(Ticks(Now), Token)) return false;
		OutAccessToken = HaivenHandoffDetail::FromUtf8(Token);
		return true;
	}

	void FHandoffCore::ClearCredential()
	{
		Core.ClearCredential();
	}

	void FHandoffCore::SetCredentialStore(TSharedPtr<IHandoffCredentialStore> Store)
	{
		StoreBridge.Target = MoveTemp(Store);
		Core.SetCredentialStore(StoreBridge.Target.IsValid() ? &StoreBridge : nullptr);
	}

	bool FHandoffCore::Resume(const FDateTime& Now, FHandoffCredential& OutCredential)
	{
		Handoff::FCredential Credential;
		if (!Core.Resume(Ticks(Now), Credential)) return false;
		OutCredential = ToEngine(Credential);
		return true;
	}

	bool FHandoffCore::FStoreBridge::Load(Handoff::FCredential& OutCredential)
	{
		FHandoffCredential Saved;
		if (!Target.IsValid() || !Target->Load(Saved)) return false;
		OutCredential = ToCore(Saved);
		return true;
	}

	void FHandoffCore::FStoreBridge::Store(const Handoff::FCredential& Credential)
	{
		if (Target.IsValid()) Target->Store(ToEngine(Credential));
	}

	void FHandoffCore::FStoreBridge::Clear()
	{
		if (Target.IsValid()) Target->Clear();
	}

	bool FHandoffCore::Restart(
		Handoff::ECredentialEndReason Reason,
		const FHandoffConfig& Config,
		TConstArrayView<uint8> StateEntropy,
		TConstArrayView<uint8> VerifierEntropy,
		const FDateTime& Now,
		FString& OutAuthorizationUrl,
		FString& OutState,
		FString& OutError)
	{
		std::vector<std::uint8_t> State = Bytes(StateEntropy);
		std::vector<std::uint8_t> Verifier = Bytes(VerifierEntropy);
		std::string Url;
		std::string StateText;
		std::string Error;
		const bool bBegan = Core.Restart(Reason, ToCore(Config), State, Verifier, Ticks(Now), Url, StateText, Error);
		Wipe(State);
		Wipe(Verifier);
		if (!bBegan)
		{
			OutError = HaivenHandoffDetail::FromUtf8(Error);
			return false;
		}
		OutAuthorizationUrl = HaivenHandoffDetail::FromUtf8(Url);
		OutState = HaivenHandoffDetail::FromUtf8(StateText);
		return true;
	}

	FString FHandoffCore::PkceChallenge(const FString& Verifier)
	{
		return HaivenHandoffDetail::FromUtf8(Handoff::PkceChallenge(HaivenHandoffDetail::ToUtf8(Verifier)));
	}
}
