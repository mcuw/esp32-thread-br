import { $, component$, useSignal, useVisibleTask$ } from '@qwik.dev/core';
import { type Neighbor, apiGet, apiPost } from '../lib/api';
import { getStoredToken } from '../lib/token';

interface NeighborWithAddr extends Neighbor {
  rloc_address: string;
}

type CoapMethod = 'GET' | 'PUT' | 'POST' | 'DELETE';

export default component$(() => {
  const neighbors = useSignal<NeighborWithAddr[]>([]);
  const selectedExtMac = useSignal('');
  const method = useSignal<CoapMethod>('GET');
  const path = useSignal('light');
  const payload = useSignal('{"on":true,"r":0,"g":255,"b":0}');
  const response = useSignal('');
  const sending = useSignal(false);

  useVisibleTask$(() => {
    const fetchNeighbors = async () => {
      try {
        neighbors.value = await apiGet<NeighborWithAddr[]>('/thread/neighbors');
      } catch (e) {
        console.error(e);
      }
    };
    fetchNeighbors();
    const interval = setInterval(fetchNeighbors, 5000);
    return () => clearInterval(interval);
  });

  const sendRequest = $(async () => {
    if (!selectedExtMac.value) {
      response.value = 'Please select a device';
      return;
    }

    const freshNeighbors =
      await apiGet<NeighborWithAddr[]>('/thread/neighbors');
    const target = freshNeighbors.find(
      (n) => n.ext_mac === selectedExtMac.value,
    );
    if (!target) {
      response.value = 'Device not found in the network';
      return;
    }

    const token = getStoredToken();

    sending.value = true;
    response.value =
      'Sending (can take a moment during network synchronization)...';

    try {
      const result = await apiPost<{
        success: boolean;
        code: number;
        payload?: string;
      }>('/thread/coap-request', token, {
        address: target.rloc_address,
        method: method.value,
        path: path.value,
        payload:
          method.value === 'PUT' || method.value === 'POST'
            ? payload.value
            : undefined,
      });
      response.value = `Code ${result.code}${result.payload ? `: ${result.payload}` : ' (kein Payload)'}`;
    } catch (e) {
      response.value = e instanceof Error ? e.message : 'Fehler';
    } finally {
      sending.value = false;
    }
  });

  return (
    <div class="card">
      <h2>CoAP-Test-Client</h2>

      <select
        onChange$={(e) => {
          selectedExtMac.value = (e.target as HTMLSelectElement).value;
        }}
      >
        <option value="">Select Device...</option>
        {neighbors.value.map((n) => (
          <option key={n.ext_mac} value={n.ext_mac}>
            {n.ext_mac}
          </option>
        ))}
      </select>

      <div class="coap-form-row">
        <select
          onChange$={(e) => {
            method.value = (e.target as HTMLSelectElement).value as CoapMethod;
          }}
        >
          <option value="GET">GET</option>
          <option value="PUT">PUT</option>
          <option value="POST">POST</option>
          <option value="DELETE">DELETE</option>
        </select>
        <input
          type="text"
          placeholder="Path (e.g. light)"
          value={path.value}
          onInput$={(e) => {
            path.value = (e.target as HTMLInputElement).value;
          }}
        />
      </div>

      {(method.value === 'PUT' || method.value === 'POST') && (
        <textarea
          placeholder='JSON-Payload, z.B. {"on":true,"r":255,"g":0,"b":0}'
          onInput$={(e) => {
            payload.value = (e.target as HTMLTextAreaElement).value;
          }}
        >
          {payload.value}
        </textarea>
      )}

      <button type="button" onClick$={sendRequest} disabled={sending.value}>
        {sending.value ? 'Sending...' : 'Send Request'}
      </button>

      {response.value && <pre class="coap-response">{response.value}</pre>}
    </div>
  );
});
