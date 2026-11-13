import { Link, Outlet } from 'react-router'

export function Layout() {
  return (
    <>
      <header className="site-header">
        <Link to="/" className="site-title">
          Chess3
        </Link>
      </header>
      <main>
        <Outlet />
      </main>
    </>
  )
}
