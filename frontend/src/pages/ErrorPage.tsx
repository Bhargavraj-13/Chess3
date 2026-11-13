import { useRouteError } from 'react-router'

// Shown instead of a blank page when rendering a route throws.
export function ErrorPage() {
  const error = useRouteError()
  console.error(error)

  return (
    <main>
      <h1>Something went wrong</h1>
      <p>Reload the page to try again.</p>
    </main>
  )
}
