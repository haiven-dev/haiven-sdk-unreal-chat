# Contributing

Contributions are welcome. This repository is public. Bug reports, fixes,
and documentation improvements are all useful.

`HaivenChatStarter.uproject` is at the repository root.

## Issues

Open a GitHub issue for a bug, a question about behavior, or a change you
want to discuss before writing it. Search open issues first.

Include the Unreal Engine version and the steps that show the problem.
Do not paste access tokens, passwords, or private endpoints.

## Pull requests

1. Fork the repository and branch from `main`.
2. Keep the change focused.
3. Use a [Conventional Commits](https://www.conventionalcommits.org/)
   subject:

   `type(scope): subject`

   `type` is one of `feat`, `fix`, `docs`, `deps`, `style`, `refactor`,
   `perf`, `test`, `build`, `ci`, `chore`, or `revert`. `scope` is
   optional.
4. Open the pull request against `main`. Say what changed and how you
   checked it.

Do not commit access tokens, passwords, private endpoints, or local
credentials. Do not put an access token in a config file.

## Checks

From the repository root, without an Unreal install:

```sh
python3 test_prepare.py
```

That test reads the engine list from a sibling checkout of the public
`haiven-sdk-unreal` repository, or from `HAIVEN_SDK_UNREAL` when that
variable is set. `prepare.py --dest <directory>` assembles a working
project. Its default engine is the primary version in that list.
