# Feature: GitHub Actions CI And Releases

## Status

Proposed. This feature adds repository automation that cross-compiles the Pico 2 W firmware for every pull request and every push to `main`, blocks merging into `main` unless the required build check passes, and publishes versioned firmware files as GitHub Releases.

No GitHub Actions workflows currently exist in this repository. The CMake target `PicoMonsterEyes` calls `pico_add_extra_outputs`, so a successful build is expected to produce the flashable firmware formats alongside `PicoMonsterEyes.elf`, including `.uf2` and `.bin`.

## Goals

- Verify firmware builds reproducibly in GitHub-hosted CI for pull requests and `main` changes.
- Require the CI build check before pull requests can merge into `main`.
- Create a version tag and GitHub Release from a deliberate release trigger.
- Attach the exact firmware files built by CI to every release.
- Preserve enough build metadata to reproduce a released artifact later.

## Workflow Design

### Continuous Integration

Add `.github/workflows/firmware-ci.yml` with these triggers:

- `pull_request` targeting `main`.
- `push` to `main`.
- Optional manual dispatch for diagnosing CI environment changes.

The workflow must:

1. Check out the exact commit being built.
2. Install a pinned ARM GNU toolchain, CMake, Ninja, and the Pico SDK dependency set required by the project.
3. Configure a clean out-of-tree build for `PICO_BOARD=pico2_w`.
4. Build the `PicoMonsterEyes` target.
5. Upload `.uf2`, `.bin`, and `.elf` as a short-retention workflow artifact, even on a manually triggered build.
6. Report the firmware size from the final ELF as build metadata.

Use a stable check name such as `firmware-build`; that exact name becomes the branch-protection requirement.

### Merge Protection

GitHub Actions can report a successful check, but it cannot independently prevent direct merges. Configure a GitHub branch protection rule or repository ruleset for `main` after the CI workflow first runs:

- Require a pull request before merging.
- Require the `firmware-build` status check to pass and be current with the target branch.
- Require conversation resolution if repository policy calls for it.
- Restrict or disable administrator bypass according to the repository owner's policy.
- Prevent direct pushes to `main` except for explicitly approved automation, if any.

The workflow file and this GitHub repository setting must be treated as one feature. Changing the workflow job name without updating the ruleset would silently remove the intended merge gate.

### Release

Add `.github/workflows/release.yml` with a manual `workflow_dispatch` trigger that accepts a version value in the form `vMAJOR.MINOR.PATCH`. A manual trigger prevents every `main` merge from becoming a public firmware release.

The release workflow must:

1. Validate the version format and confirm that the requested tag does not already exist.
2. Check out the selected `main` commit and verify it is reachable from `main`.
3. Run the same pinned configure-and-build steps as the CI workflow.
4. Fail the release if the firmware build fails.
5. Create an annotated Git tag at the built commit and push it using the workflow's least-privilege `contents: write` permission.
6. Create a GitHub Release for that tag.
7. Upload `PicoMonsterEyes.uf2`, `PicoMonsterEyes.bin`, and `PicoMonsterEyes.elf` to the release.
8. Include the source commit SHA, Pico SDK revision, board (`pico2_w`), toolchain version, and file checksums in the release notes or a build-manifest attachment.

The `.uf2` file is the primary end-user artifact for USB BOOTSEL flashing. The `.elf` supports debugging and SWD workflows; the `.bin` is retained for tool compatibility.

## Dependency And Security Policy

- Pin GitHub Actions to immutable commit SHAs, with version comments, rather than floating action tags.
- Pin the Pico SDK revision used by CI; do not depend on an unpinned `master` checkout.
- If `pico-extras` becomes necessary for Bluetooth audio, pin and record its revision in CI and release metadata too.
- Use read-only workflow permissions by default. Grant `contents: write` only to the release job that creates tags and releases.
- Do not expose secrets in build output. The planned public-release workflow should require no custom secret when using the repository-provided `GITHUB_TOKEN`.
- Keep third-party toolchain downloads checksum-verified or obtained from a trusted GitHub Action/toolchain distribution.

## Milestones

1. Decide and document the pinned Pico SDK, toolchain, CMake, Ninja, and action revisions.
2. Add the CI workflow and verify it on a pull request.
3. Configure the `main` branch ruleset requiring the `firmware-build` check.
4. Confirm the CI artifact contains valid `.uf2`, `.bin`, and `.elf` outputs for `pico2_w`.
5. Add the manual release workflow and run a pre-release tag on a test commit.
6. Verify the release assets, checksums, tag target, and release metadata from a clean clone.
7. Document the maintainer release procedure and recovery steps for a failed release.

## Acceptance Criteria

- Every pull request targeting `main` reports a passing or failing `firmware-build` check.
- Every push to `main` runs the same firmware build.
- The `main` branch ruleset prevents merging a pull request until the current `firmware-build` check passes.
- A manual release with a new valid semantic version creates exactly one annotated tag and GitHub Release at the chosen `main` commit.
- Each release includes verified `.uf2`, `.bin`, and `.elf` files plus reproducibility metadata and checksums.
- A failed build, duplicate version, invalid version, or non-`main` release commit creates neither a tag nor a GitHub Release.

## Out Of Scope

- Automated semantic-version calculation, release-note generation from pull requests, signed artifacts, hardware-in-the-loop flashing, and publishing a package to an external registry are deferred from the first release automation iteration.
