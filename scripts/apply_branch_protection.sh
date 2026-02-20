#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: ./scripts/apply_branch_protection.sh [owner/repo] [profile]

Profiles:
  single-maintainer  Safe default for solo maintainers (default)
  team               Enforced reviews/admin checks for team repos

Examples:
  ./scripts/apply_branch_protection.sh EnesGoker/sfml-2048
  ./scripts/apply_branch_protection.sh EnesGoker/sfml-2048 team
EOF
}

if ! command -v gh >/dev/null 2>&1; then
  echo "Missing required command: gh"
  exit 1
fi

if ! gh auth status >/dev/null 2>&1; then
  echo "GitHub CLI is not authenticated. Run: gh auth login"
  exit 1
fi

repo="${1:-}"
profile="${2:-single-maintainer}"

if [[ "${repo:-}" == "-h" || "${repo:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ "${profile:-}" == "-h" || "${profile:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [ -z "${repo:-}" ]; then
  origin_url="$(git remote get-url origin 2>/dev/null || true)"
  if [ -n "$origin_url" ]; then
    repo="$(printf '%s' "$origin_url" | sed -E 's#(git@github.com:|https://github.com/)##; s#\.git$##')"
  fi
fi

if [[ -z "$repo" || "$repo" != */* ]]; then
  usage
  exit 1
fi

case "$profile" in
  single-maintainer)
    gh api \
      --method PUT \
      --header "Accept: application/vnd.github+json" \
      "repos/${repo}/branches/main/protection" \
      --input - <<'JSON'
{
  "required_status_checks": {
    "strict": true,
    "contexts": [
      "Format Check",
      "Static Analysis (ubuntu-latest)",
      "Build and Test (ubuntu-latest)",
      "Build and Test (macos-latest)",
      "Build and Test (windows-latest)",
      "Sanitizers (ubuntu-latest)",
      "Coverage (ubuntu-latest)"
    ]
  },
  "enforce_admins": false,
  "required_pull_request_reviews": null,
  "restrictions": null,
  "required_linear_history": true,
  "allow_force_pushes": false,
  "allow_deletions": false,
  "block_creations": false,
  "required_conversation_resolution": true,
  "lock_branch": false,
  "allow_fork_syncing": true
}
JSON
    ;;
  team)
    gh api \
      --method PUT \
      --header "Accept: application/vnd.github+json" \
      "repos/${repo}/branches/main/protection" \
      --input - <<'JSON'
{
  "required_status_checks": {
    "strict": true,
    "contexts": [
      "Format Check",
      "Static Analysis (ubuntu-latest)",
      "Build and Test (ubuntu-latest)",
      "Build and Test (macos-latest)",
      "Build and Test (windows-latest)",
      "Sanitizers (ubuntu-latest)",
      "Coverage (ubuntu-latest)"
    ]
  },
  "enforce_admins": true,
  "required_pull_request_reviews": {
    "dismiss_stale_reviews": true,
    "require_code_owner_reviews": true,
    "required_approving_review_count": 1,
    "require_last_push_approval": false
  },
  "restrictions": null,
  "required_linear_history": true,
  "allow_force_pushes": false,
  "allow_deletions": false,
  "block_creations": false,
  "required_conversation_resolution": true,
  "lock_branch": false,
  "allow_fork_syncing": true
}
JSON
    ;;
  *)
    echo "Unknown profile: ${profile}"
    usage
    exit 1
    ;;
esac

echo "Branch protection applied for ${repo}:main (profile: ${profile})"
