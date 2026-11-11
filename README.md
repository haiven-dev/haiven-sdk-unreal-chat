# Unreal chat starter

A minimal, source-only Unreal project that exercises the public Haiven plugin
surface introduced in [PR #83](https://github.com/haiven-dev/haiven-sdk/pull/83).
The code-built UMG panel connects through `UHaivenSubsystem`, consumes typed
`UHaivenRealtimeEvents` message and typing delegates, and calls
`SendChatMessage`. One line follows each outgoing message through `sending`,
`unconfirmed` (kept for reconnect), `accepted`, `held`, `removed`, or `refused`.
Connection, socket, configuration, and immediate-send failures stay visible in
the panel rather than only in the Unreal log.

The project contains no binary assets and no credential. It runs the engine's
built-in Entry map under `AHaivenChatStarterGameMode`, so every part of the
sample can be reviewed and merged as text.

## Supported engines

| Engine | CI runner | Status |
|---|---|---|
| Unreal Engine 5.8.2 | `unreal-5.8.2-clang20` | Plugin, starter, fixture, and panel automation |
| Unreal Engine 5.6.1 | `unreal-5.6.1-clang18` | Plugin, starter, fixture, and panel automation |

These are the exact versions in `sdks/unreal/ci/engines.json`. The existing
`unreal-editor` workflow builds a clean host with both the Haiven and starter
plugins and runs `Haiven.ChatStarter.*` on both matrix rows.

## Install and run

From the repository root, assemble a disposable project for an installed
supported engine:

```sh
python3 demos/unreal-chat-starter/prepare.py \
  --engine 5.8.2 \
  --dest /tmp/haiven-chat-starter
```

Edit `/tmp/haiven-chat-starter/Config/DefaultGame.ini` and set `BaseUrl`,
`RealtimeUrl`, `AppId`, and `ChannelId`. Obtain a short-lived demo access token
through the environment's documented authentication flow and expose it only to
the editor process:

```sh
export HAIVEN_DEMO_ACCESS_TOKEN='...'
/path/to/UnrealEditor /tmp/haiven-chat-starter/HaivenChatStarter.uproject
```

Press Play. The panel connects automatically; the Reconnect button starts a new
socket after a close, and Send Chat Message (or Enter in the input) uses the
reconnect-resilient SDK outbox. Never put the access token in `DefaultGame.ini`.

## Verification

Without Unreal installed:

```sh
python3 demos/unreal-chat-starter/test_prepare.py
python3 sdks/unreal/ci/test_reports.py
node sdks/unreal/validate-plugin.mjs
```

With either matrix engine installed, run the same clean staging and automation
used by CI:

```sh
python3 sdks/unreal/ci/run.py 5.8.2
python3 sdks/unreal/ci/run.py 5.6.1
```

`Haiven.ChatStarter.Fixture` parses checked-in `chat.message` and `chat.typing`
payloads into the generated typed contracts and covers every outbox result,
including reconnect and refusal diagnostics. `Haiven.ChatStarter.Panel` proves
the source-built UI exposes Message Input, Send Chat Message, Reconnect, and a
keyed outbox log. `Haiven.ChatStarter.Settings` proves incomplete connection
configuration is rejected.

## Haiven versioning

haiven-sdk and haiven-sdk-unreal follow semver. A feature of the Unreal target ships in the shared major.minor release, in parity with the other target SDKs. A patch of haiven-sdk-unreal is a fix and can ship on its own.

This demo, haiven-sdk-unreal-chat, publishes that same major.minor and does not follow semver. A feature that shows part of the platform in this chat starter can be introduced in a patch. That patch leaves haiven-sdk and haiven-sdk-unreal unchanged.

The proposal is [haiven-docs ADR 0051](https://github.com/haiven-dev/haiven-docs/blob/docs/adr-0051-major-minor-lockstep/decisions/0051-major-minor-lockstep.md).
