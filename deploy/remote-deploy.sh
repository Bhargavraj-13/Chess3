#!/usr/bin/env bash
# Runs on the VPS, started over SSH by .github/workflows/deploy.yml.
#
#   remote-deploy.sh deploy <image-prefix> <image-tag> <domain> <registry-user>
#       Reads the registry token from stdin.
#   remote-deploy.sh rollback
#       Restores the version that was running before the last deploy.
set -euo pipefail
cd "$(dirname "$0")"

compose() {
  docker compose --env-file "$1" -f compose.yaml "${@:2}"
}

tag_in() {
  [ -f "$1" ] && sed -n 's/^IMAGE_TAG=//p' "$1"
}

remove_images() {
  local env_file=$1 tag prefix
  [ -f "$env_file" ] || return 0
  tag=$(tag_in "$env_file")
  prefix=$(sed -n 's/^IMAGE_PREFIX=//p' "$env_file")
  docker image rm "$prefix-server:$tag" "$prefix-web:$tag" >/dev/null 2>&1 || true
}

deploy() {
  local prefix=$1 tag=$2 domain=$3 user=$4

  printf 'IMAGE_PREFIX=%s\nIMAGE_TAG=%s\nDOMAIN=%s\n' "$prefix" "$tag" "$domain" > .env.next

  docker login ghcr.io --username "$user" --password-stdin
  trap 'docker logout ghcr.io >/dev/null 2>&1 || true' EXIT
  compose .env.next pull

  # Keep one earlier version for rollback, and remove the one before that.
  if [ -f .env ] && [ "$(tag_in .env)" != "$tag" ]; then
    if [ -f .env.previous ] && [ "$(tag_in .env.previous)" != "$tag" ]; then
      remove_images .env.previous
    fi
    cp .env .env.previous
  fi
  mv .env.next .env

  if ! compose .env up -d --remove-orphans; then
    echo "The new version failed to start, so restoring the previous one" >&2
    rollback
    exit 1
  fi
  docker image prune -f >/dev/null
}

rollback() {
  if [ ! -f .env.previous ]; then
    echo "No earlier version to roll back to" >&2
    exit 1
  fi

  mv .env .env.failed
  mv .env.previous .env
  compose .env up -d --remove-orphans
  remove_images .env.failed
  rm -f .env.failed
}

case "${1:-}" in
  deploy)
    [ "$#" -eq 5 ] || { echo "deploy needs 4 arguments" >&2; exit 2; }
    deploy "$2" "$3" "$4" "$5"
    ;;
  rollback)
    rollback
    ;;
  *)
    echo "Usage: remote-deploy.sh deploy|rollback" >&2
    exit 2
    ;;
esac
