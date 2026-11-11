# Unreal chat starter

A minimal, source-only Unreal project that exercises the public Haiven plugin
surface introduced in PR #83.
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

`prepare.py` reads those versions from a sibling `haiven-sdk-unreal` checkout,
or from `HAIVEN_SDK_UNREAL` when that variable is set (`ci/engines.json`).

## Install and run

From the repository root, assemble a disposable project for an installed
supported engine:

```sh
python3 prepare.py \
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

Without Unreal installed, point `HAIVEN_SDK_UNREAL` at a `haiven-sdk-unreal`
checkout when it is not the sibling directory, then:

```sh
python3 test_prepare.py
```

`.github/workflows/build.yml` runs that test on pull requests and on a `v*` tag.
It checks out `haiven-sdk-unreal` at the commit named in the workflow. The
plugin repository's own `python3 ci/run.py` is the editor build; it is not in
this tree.

`Haiven.ChatStarter.Fixture` parses checked-in `chat.message` and `chat.typing`
payloads into the generated typed contracts and covers every outbox result,
including reconnect and refusal diagnostics. `Haiven.ChatStarter.Panel` proves
the source-built UI exposes Message Input, Send Chat Message, Reconnect, and a
keyed outbox log. `Haiven.ChatStarter.Settings` proves incomplete connection
configuration is rejected.

## Binary release

`.github/workflows/release-binaries.yml` compiles the vendored Haiven plugin
and HaivenChatStarter for each engine in `ci/engines.json`. It does not cook a
game and it does not build a Fab package. The source archive, when that
workflow is on the release branch, stays a separate job.

The editor build ships `Binaries/Linux/libUnrealEditor-*.so`. Game Development
and Shipping are monolithic, so the workflow compiles each plugin with UBT
`-plugin=` and ships `<Module>.precompiled` plus the object files it lists. It
does not link an UnrealGame executable. Checksums are `SHA256SUMS-binaries`,
leaving the source archive's `SHA256SUMS` in place.

The tag must be `v` plus `VersionName` in
`Plugins/HaivenChatStarter/HaivenChatStarter.uplugin`. The vendored
`Plugins/Haiven/Haiven.uplugin` stays at `0.0.0`. A dispatch without `publish`
uploads the workflow artifact only.

Each asset is `haiven-unreal-chat-<version>-UE<major.minor>-Linux.zip`. Unzip
it over the source tree. The packaged plugins are `Installed` and stamped with
`EngineVersion` `<major>.<minor>.0`, so they load in the engine that built
them. Linux is the only host: Unreal compiles Win64 on Windows and Mac on
macOS, and this repository does not run those jobs.
