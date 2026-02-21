# Maintenance Playbook

This document defines the routine maintenance and optional hardening steps for this repository.

## A. Routine Maintenance

### Weekly

1. Review open Dependabot PRs.
2. Confirm CI is green on each PR.
3. Merge safe dependency updates through PR flow.

Commands:

```bash
gh pr list --state open --limit 30
gh pr view <PR_NUMBER>
```

### Per Feature/Bugfix

1. Create a branch from `main`.
2. Implement the change.
3. Open a PR and wait for required checks.
4. Merge only after CI is green.

Commands:

```bash
git checkout -b feat/<name>
# edit
git add .
git commit -m "feat: <summary>"
git push -u origin feat/<name>
```

## B. Optional Hardening (Cila)

Apply these gradually to avoid blocking solo development.

1. Signed commits (GPG/SSH/Sigstore).
2. Tighten review rules (for team mode).
3. Supply-chain hardening (attestation, advanced SBOM workflows).

## Guardrails

1. Do not push directly to `main`; use PRs.
2. Keep required checks stable before changing branch protection.
3. Treat broken CI as highest-priority maintenance work.
