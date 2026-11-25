# Unreal chat starter

A complete, source-only Unreal project with the Haiven plugin included.
The code-built UMG panel connects through `UHaivenSubsystem`, consumes typed
`UHaivenRealtimeEvents` message and typing delegates, and calls
`SendChatMessage`. One line follows each outgoing message through `sending`,
`unconfirmed` (kept for reconnect), `accepted`, `held`, `removed`, or `refused`.
Connection, socket, configuration, and immediate-send failures stay visible in
the panel rather than only in the Unreal log.

The project contains no binary assets and no credential. It runs the engine's
built-in Entry map under `AHaivenChatStarterGameMode`, so every part of the
sample can be reviewed and merged as text.

## Set up the demo

1. Install Unreal Engine **5.8.2** and the C++ toolchain for your platform.
2. Clone this repository or extract the source ZIP from
   [Releases](https://github.com/haiven-dev/haiven-sdk-unreal-chat/releases).
   Start the installed Unreal Editor **5.8.2** and open
   `HaivenChatStarter.uproject` from its project browser. You can also pass
   the project path to the editor executable for **5.8.2**. Build the modules
   when Unreal prompts you.
3. Press **Play** to open the chat panel. For a live connection, set
   `BaseUrl`, `RealtimeUrl`, `AppId`, and `ChannelId` in the
   `[/Script/HaivenChatStarter.HaivenChatStarterSettings]` section of
   `Config/DefaultGame.ini`.
4. Obtain a short-lived demo access token and set `HAIVEN_DEMO_ACCESS_TOKEN`
   in the environment of the editor process. Keep the token out of
   `DefaultGame.ini`. Launch Unreal Editor from that environment by passing
   it the path to `HaivenChatStarter.uproject`. Press **Play** and the panel
   connects automatically.

Use **Reconnect** after a socket closes. Send a message with **Send Chat
Message** or **Enter** while typing in the message input.

## Releases

Releases are published on this repository's
[GitHub Releases page](https://github.com/haiven-dev/haiven-sdk-unreal-chat/releases).
This repository is published from Haiven's SDK source, and Haiven's release
pipeline builds and publishes every release from that source after a
maintainer review. Nothing in this repository cuts a release: there is no
release workflow, release pull request, or tag to push here.

Each release carries `haiven-unreal-chat-starter-source-<version>.zip` and
`SHA256SUMS`, the SHA-256 digest of every asset in `sha256sum` format. The
archive's top-level folder is `HaivenChatStarter/` and contains this project
(`HaivenChatStarter.uproject`, `Config/`, `Plugins/`, and this README) with
`Binaries`, `Intermediate`, `Saved`, and `DerivedDataCache` removed. It is
source only: no compiled binaries, no cooked game, no Fab package, and no Epic
content. Follow the [setup steps](#set-up-the-demo) to select the editor and
build the project. The release notes list each asset with its digest and name
the source commit it was built from.

## Supported engines

| Engine | CI runner | Status |
|---|---|---|
| Unreal Engine 5.8.2 | `unreal-5.8.2-clang20` | Plugin, starter, fixture, and panel automation |
| Unreal Engine 5.6.1 | `unreal-5.6.1-clang18` | Plugin, starter, fixture, and panel automation |

The shipped `HaivenChatStarter.uproject` leaves `EngineAssociation` empty.
For **5.6.1**, follow the setup steps using the **5.6.1** editor and its
toolchain. To create a separate working copy associated with an engine version,
extract the source ZIP and run
`python3 prepare.py --dest ../HaivenChatStarter-local --engine 5.8.2` from the
extracted starter folder. Use `--engine 5.6.1` for the other supported version.

## Haiven versioning

A release is tagged `v<version>`. Haiven's SDK and demo repositories share one
version line, which follows [Semantic Versioning](https://semver.org/): the
repositories in one release carry the same version, so this starter's release
has the same major.minor as the
[haiven-sdk-unreal](https://github.com/haiven-dev/haiven-sdk-unreal) release it
ships with. A release can cover only some of the repositories, so a fix or
feature that only affects this starter can ship without a haiven-sdk-unreal
release, and this repository's version numbers can skip. A version with a
pre-release suffix (`v1.2.3-rc.1`) is published as a GitHub pre-release and is
never marked Latest.

The release tag and the asset name carry the version. `VersionName` in
`Plugins/HaivenChatStarter/HaivenChatStarter.uplugin` is not the release
number; read the release tag instead.

Source updates arrive as pull requests into `main`. Contributions are
welcome as pull requests here too; see [CONTRIBUTING.md](CONTRIBUTING.md).
Haiven's maintainers carry each accepted change into Haiven's source, so later
updates keep it.

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

## License

Haiven's source in this repository, the starter project and the plugins under
`Plugins/`, is licensed under the Apache License, Version 2.0 (SPDX
`Apache-2.0`). The terms are in [LICENSE](LICENSE); `Plugins/Haiven` carries
the same text in its own `LICENSE.md`. Contributions are accepted under that
license, as [CONTRIBUTING.md](CONTRIBUTING.md) says.

Unreal Engine is not part of that license and is not in this repository. It is
covered by the [Unreal Engine EULA](https://www.unrealengine.com/eula/unreal).

<!-- features:start (generated by codegen/features.py; do not edit) -->

### Feature coverage

This table summarizes the SDK used by this demo. The demo exercises the features described in its setup and examples.

| Family | Unreal |
|---|:---:|
| Envelope | ~ |
| Session lifecycle | ~ |
| Request recovery | ✅ |
| Open vocabularies | ~ |
| REST | ○ |
| Game login handoff | ○ |
| P2P sessions | ○ |
| Credential lifecycle | ✅ |
| API surface | ✅ |
| Product | ~ |
| Developer experience | ~ |

✅ shipped · `↑` inherited from the language SDK this target ships, proven only in that SDK's runner and not inside the engine · `~` partial · `—` absent · `○` not yet proven by any scenario · `⊘` not applicable. A family cell is ✅ only when every counted row is ✅, and `↑` when every counted row is ✅ or `↑`; `MAY` requirements and rows marked optional are not counted. Every row: the full Haiven SDK feature matrix.

<!-- features:end -->
