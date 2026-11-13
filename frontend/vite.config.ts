import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'

// In production, Caddy sends /ws and /healthz to the C++ server (deploy/Caddyfile).
// These proxies do the same in development, so the app can always use
// same-origin URLs like `/ws`. Start chess_server with PORT=8080 to use them.
const backend = 'http://localhost:8080'
const proxy = {
  '/ws': { target: backend, ws: true },
  '/healthz': backend,
}

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  server: { proxy },
  preview: { proxy },
})
