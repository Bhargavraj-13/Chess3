import { Link } from 'react-router'

// Placeholder. The create game and join game forms come in #15.
export function HomePage() {
  return (
    <>
      <h1>Chess3</h1>
      <p>Play chess with a friend. One player creates a game and shares the code.</p>
      <Link to="/game">Go to the game page</Link>
    </>
  )
}
