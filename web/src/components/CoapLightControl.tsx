import { $, component$, useSignal, useVisibleTask$ } from '@qwik.dev/core';
import { type Neighbor, apiGet, apiPost } from '../lib/api';
import { getStoredToken } from '../lib/token';

interface NeighborWithAddr extends Neighbor {
  rloc_address: string;
}

export default component$(() => {
  const neighbors = useSignal<NeighborWithAddr[]>([]);
  const selectedExtMac = useSignal('');
  const color = useSignal('#ffffff');
  const message = useSignal('');

  useVisibleTask$(async () => {
    neighbors.value = await apiGet<NeighborWithAddr[]>('/thread/neighbors');

    const fetchNeighbors = async () => {
      try {
        neighbors.value = await apiGet<NeighborWithAddr[]>('/thread/neighbors');
      } catch (e) {
        console.error('Neighbor-Fetch fehlgeschlagen:', e);
      }
    };
    fetchNeighbors();
    const interval = setInterval(fetchNeighbors, 5000);
    return () => clearInterval(interval);
  });

  const setLight = $(async (on: boolean) => {
    if (!selectedExtMac.value) {
      message.value = 'Please select a device';
      return;
    }

    // Frische Adresse holen statt der evtl. veralteten aus dem Dropdown
    message.value = 'Searching for current address...';
    const freshNeighbors =
      await apiGet<NeighborWithAddr[]>('/thread/neighbors');
    const target = freshNeighbors.find(
      (n) => n.ext_mac === selectedExtMac.value,
    );
    if (!target) {
      message.value = 'Device not found in the network';
      return;
    }
    const token = getStoredToken();
    const hex = color.value.replace('#', '');
    const r = Number.parseInt(hex.substring(0, 2), 16);
    const g = Number.parseInt(hex.substring(2, 4), 16);
    const b = Number.parseInt(hex.substring(4, 6), 16);

    message.value = 'Sending...';
    try {
      await apiPost('/thread/coap-light', token, {
        address: target.rloc_address,
        on,
        r,
        g,
        b,
      });
      message.value = 'OK';
    } catch (e) {
      message.value = e instanceof Error ? e.message : 'Error';
    }
  });

  return (
    <div class="card">
      <h2>CoAP - Light Control</h2>
      <select
        onChange$={(e) => {
          selectedExtMac.value = (e.target as HTMLSelectElement).value;
        }}
      >
        <option value="">Choose device...</option>
        {neighbors.value.map((n) => (
          <option key={n.ext_mac} value={n.ext_mac}>
            {n.ext_mac}
          </option>
        ))}
      </select>
      <label>
        Color
        <input
          type="color"
          value={color.value}
          onInput$={(e) => {
            color.value = (e.target as HTMLInputElement).value;
          }}
        />
      </label>
      <div class="button-row">
        <button type="button" onClick$={() => setLight(true)}>
          On
        </button>
        <button type="button" onClick$={() => setLight(false)}>
          Off
        </button>
      </div>
      {message.value && <p class="muted">{message.value}</p>}
    </div>
  );
});
