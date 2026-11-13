import type { RouteObject } from 'react-router'
import { ErrorPage } from './pages/ErrorPage.tsx'
import { GamePage } from './pages/GamePage.tsx'
import { HomePage } from './pages/HomePage.tsx'
import { Layout } from './pages/Layout.tsx'
import { NotFoundPage } from './pages/NotFoundPage.tsx'

// Kept separate from the router itself, so tests can render these routes in a
// memory router without a browser.
//
// Caddy serves index.html for any path that isn't a file, so every route here
// also works when the page is refreshed or opened from a link.
//
// Never put a challenge code or player token in the path or query string: the
// server sees both. See section 6 of docs/system-design.md.
export const routes: RouteObject[] = [
  {
    path: '/',
    Component: Layout,
    ErrorBoundary: ErrorPage,
    children: [
      { index: true, Component: HomePage },
      { path: 'game', Component: GamePage },
      { path: '*', Component: NotFoundPage },
    ],
  },
]
