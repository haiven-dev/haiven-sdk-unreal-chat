#!/usr/bin/env bash
# Prints conventional-commit subjects in a promotion that release-please would
# skip because their commit dates precede the latest release on the base.
set -euo pipefail

if (( $# != 3 )); then
  echo "usage: $0 <base> <head> <tag-glob>" >&2
  exit 2
fi

base=$1
head=$2
tag_glob=$3
released_at=0
conventional_subject='^[a-z]+(\([^)]+\))?!?:[[:space:]]'

while IFS= read -r tag; do
  [[ -z "$tag" ]] && continue
  tag_time="$(git log -1 --format=%ct "$tag^{commit}")"
  if (( tag_time > released_at )); then
    released_at=$tag_time
  fi
done < <(git tag --merged "$base" --list "$tag_glob")

git log --no-merges --format='%H%x09%ct%x09%s' "$base..$head" |
  while IFS=$'\t' read -r sha commit_time subject; do
    (( commit_time < released_at )) || continue

    patch_id="$(git show "$sha" | git patch-id --stable | awk 'NR == 1 { print $1 }')"
    if [[ -n "$patch_id" ]] &&
      git log -p --no-merges --since="@$commit_time" "$base" |
        git patch-id --stable |
        awk -v wanted="$patch_id" '$1 == wanted { found = 1 } END { exit !found }'; then
      continue
    fi

    if [[ "$subject" =~ $conventional_subject ]]; then
      printf '%s\n' "$subject"
    fi
  done | sort -u
