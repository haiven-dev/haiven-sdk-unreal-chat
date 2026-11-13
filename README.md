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

## Source release

`VersionName` in `Plugins/HaivenChatStarter/HaivenChatStarter.uplugin` is the
release version. A `v*` tag that matches it (`v` plus that version) publishes
`haiven-unreal-chat-source-<version>.zip` and `SHA256SUMS`. The archive's
top-level folder is `HaivenChatStarter/` and contains this project
(`HaivenChatStarter.uproject`, `Config/`, `Plugins/`, and this README) with
`Binaries`, `Intermediate`, `Saved`, and `DerivedDataCache` removed. It is
source only: no cooked game, no Fab package, and no Epic content. Open
`HaivenChatStarter.uproject` in the engine named by its `EngineAssociation`.

## Supported engines

| Engine | CI runner | Status |
|---|---|---|
| Unreal Engine 5.8.2 | `unreal-5.8.2-clang20` | Plugin, starter, fixture, and panel automation |
| Unreal Engine 5.6.1 | `unreal-5.6.1-clang18` | Plugin, starter, fixture, and panel automation |

`HaivenChatStarter.uproject` sets `EngineAssociation` to `5.8`. Open it in
Unreal Editor 5.8. To open it in Unreal Editor 5.6, set `EngineAssociation` to
`5.6` first.

## Install and run

Use the folder that contains this README and `HaivenChatStarter.uproject`.
In the published zip that folder is `HaivenChatStarter/`. `Plugins/Haiven` is
already in that folder.

Open `HaivenChatStarter.uproject` in the Unreal Editor 5.8 named by
`EngineAssociation`. Edit `Config/DefaultGame.ini` and set `BaseUrl`,
`RealtimeUrl`, `AppId`, and `ChannelId` under
`[/Script/HaivenChatStarter.HaivenChatStarterSettings]`. Obtain a short-lived
demo access token through your environment's documented authentication flow
and expose it only to the editor process:

```sh
export HAIVEN_DEMO_ACCESS_TOKEN='...'
/path/to/UnrealEditor HaivenChatStarter.uproject
```

Press Play. The panel connects automatically; the Reconnect button starts a new
socket after a close, and Send Chat Message (or Enter in the input) uses the
reconnect-resilient SDK outbox. Never put the access token in `DefaultGame.ini`.

## Verification

The source archive does not include a separate test runner. In that same 5.8
editor, open Test Automation and run the `Haiven.ChatStarter` filter. These
editor tests ship in the starter plugin. Fixture, Panel, and Settings do not
contact a service.

`Haiven.ChatStarter.Fixture` parses checked-in `chat.message` and `chat.typing`
payloads into the generated typed contracts and covers every outbox result in
`Content/Fixtures/chat-session.json` (`sending`, `unconfirmed`, `accepted`,
`held`, `removed`, and `refused`), plus the offline reconnect prompt.
`Haiven.ChatStarter.Panel` proves the source-built UI exposes Message Input,
Send Chat Message, Reconnect, and a keyed outbox log.
`Haiven.ChatStarter.Settings` proves incomplete connection configuration is
rejected. `Haiven.ChatStarter.ReconnectRetainsOutbox` proves a second connect
with the same demo bearer keeps an unconfirmed outbox entry. That test sets
`HAIVEN_DEMO_ACCESS_TOKEN` itself and uses a closed loopback port.

## Binary release

`.github/workflows/release-binaries.yml` compiles the vendored Haiven plugin
and HaivenChatStarter for each engine in the Supported engines table. It does
not cook a game and it does not build a Fab package. The source archive is a
separate workflow, `.github/workflows/release.yml`.

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
