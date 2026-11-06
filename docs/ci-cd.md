# CI/CD

GitHub Actions tests every pull request. Every push to `main` is tested again, then published and deployed.

```text
Pull request  -> CI
Push to main  -> CI -> publish images to GHCR -> deploy to VPS -> check live site -> roll back on failure
```

Deploying restarts the server, and games are stored in memory, so **merging to `main` ends any games in progress**.

## What CI checks

| Job | Runs when | What it does |
|---|---|---|
| C++ formatting | `CMakeLists.txt` and `.clang-format` exist | Runs `clang-format` on tracked C++ files |
| C++ | `CMakeLists.txt` exists | Builds and tests with GCC and Clang, treating warnings as errors |
| C++ sanitizers | `CMakeLists.txt` exists | Runs tests under ASan with UBSan, and under TSan |
| Frontend | `frontend/package.json` exists | Runs `npm ci`, then lint, typecheck, and test scripts if present, then build |
| Container images | Backend and frontend both exist | Checks that both images still build |
| CI passed | Always | Passes only if nothing above failed |

## Project conventions the pipeline expects

**Backend**
- `CMakeLists.txt` is at the repo root and uses `include(CTest)`, so `BUILD_TESTING=OFF` skips tests.
- The server target is `chess_server`, with `install(TARGETS chess_server)`.
- The server listens on the port in the `PORT` environment variable.
- The server answers `GET /healthz` with HTTP 200.
- The WebSocket endpoint is `/ws`.
- Slow perft tests have the `perft` CTest label, so they run only in Release jobs.
- System dependencies other than Boost must be added to `ci.yml` and `deploy/Dockerfile.server`.

**Frontend**
- The app is in `frontend/` and uses npm, with `package-lock.json` committed.
- `npm run build` writes to `frontend/dist`.

## Turning on deployment

Publishing starts as soon as the backend and frontend both exist. Deployment stays off until these steps are done.

1. Point the domain's DNS at the VPS, and open ports 22, 80, and 443.
2. Install Docker and the Compose plugin on the VPS.
3. Create a deploy user in the `docker` group, with an SSH key made only for deployment. Docker group access is effectively root on that VPS, so use it for nothing else.
4. In the repo's **Settings > Environments**, create an environment named `production` with these secrets:

   | Secret | Value |
   |---|---|
   | `VPS_HOST` | The VPS hostname or IP address |
   | `VPS_USER` | The deploy user |
   | `VPS_SSH_KEY` | The private deploy key |
   | `VPS_KNOWN_HOSTS` | The output of `ssh-keyscan <host>`, checked against the VPS |

5. In **Settings > Secrets and variables > Actions > Variables**, add these repository variables:

   | Variable | Value |
   |---|---|
   | `DOMAIN` | The site's domain, such as `chess.example.com` |
   | `DEPLOY_ENABLED` | `true` |

## Rolling back

A failed live site check restores the previous version automatically.

To go back by hand, open **Actions > Deploy > Run workflow** on `main` and enter the full commit SHA of an earlier published version.

## Protecting `main`

In **Settings > Branches**, require pull requests for `main`, and require the `CI passed` status check. It appears in the list after CI has run once.
