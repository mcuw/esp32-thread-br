import { component$, useSignal, useVisibleTask$ } from '@qwik.dev/core';
import { type Neighbor, apiGet } from '../lib/api';

export default component$(() => {
  const neighbors = useSignal<Neighbor[]>([]);
  const lastUpdate = useSignal<string>('');

  useVisibleTask$(({ cleanup }) => {
    const fetchNeighbors = async () => {
      try {
        neighbors.value = await apiGet<Neighbor[]>('/thread/neighbors');
        lastUpdate.value = new Date().toLocaleTimeString();
      } catch (e) {
        console.error(e);
      }
    };
    fetchNeighbors();
    const interval = setInterval(fetchNeighbors, 5000);
    cleanup(() => clearInterval(interval));
  });

  return (
    <div class="card">
      <h2>Thread-Nachbarn ({neighbors.value.length})</h2>
      {neighbors.value.length === 0 ? (
        <p>Keine Nachbarn gefunden.</p>
      ) : (
        <table>
          <thead>
            <tr>
              <th>RLOC16</th>
              <th>MAC</th>
              <th>RSSI</th>
              <th>Typ</th>
            </tr>
          </thead>
          <tbody>
            {neighbors.value.map((n) => (
              <tr key={n.ext_mac}>
                <td>
                  <code>0x{n.rloc16.toString(16)}</code>
                </td>
                <td>
                  <code>{n.ext_mac}</code>
                </td>
                <td>{n.avg_rssi} dBm</td>
                <td>{n.is_child ? 'Child' : 'Router'}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
      <p class="muted">Aktualisiert: {lastUpdate.value}</p>
    </div>
  );
});
