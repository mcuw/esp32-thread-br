import { $, component$, useSignal, useVisibleTask$ } from '@qwik.dev/core';
import { type Neighbor, apiGet, apiPost } from '../lib/api';

interface NeighborWithAddr extends Neighbor {
  rloc_address: string;
}

export default component$(() => {
  const neighbors = useSignal<NeighborWithAddr[]>([]);
  const selectedAddress = useSignal('');
  const sending = useSignal(false);
  const message = useSignal('');

  useVisibleTask$(async () => {
    neighbors.value = await apiGet<NeighborWithAddr[]>('/thread/neighbors');
  });

  const sendCommand = $(async (command: string) => {
    if (!selectedAddress.value) {
      message.value = 'Bitte zuerst ein Gerät auswählen';
      return;
    }
    const token = localStorage.getItem('ot_br_setup_token') ?? '';
    sending.value = true;
    try {
      await apiPost('/thread/send-command', token, {
        address: selectedAddress.value,
        command,
      });
      message.value = `"${command}" gesendet`;
    } catch (e) {
      message.value = e instanceof Error ? e.message : 'Fehler beim Senden';
    } finally {
      sending.value = false;
    }
  });

  return (
    <div class="card">
      <h2>Gerätesteuerung</h2>
      {neighbors.value.length === 0 ? (
        <p class="muted">Keine Geräte im Netzwerk gefunden.</p>
      ) : (
        <>
          <select
            onChange$={(e) => {
              selectedAddress.value = (e.target as HTMLSelectElement).value;
            }}
          >
            <option value="">Gerät wählen...</option>
            {neighbors.value.map((n) => (
              <option
                key={n.ext_mac}
                value={n.rloc_address}
                label={`${n.ext_mac} (${n.is_child ? 'Child' : 'Router'})`}
              />
            ))}
          </select>
          <div class="button-row">
            <button
              type="button"
              onClick$={() => sendCommand('TOGGLE')}
              disabled={sending.value}
            >
              Umschalten
            </button>
            <button
              type="button"
              onClick$={() => sendCommand('LED_ON')}
              disabled={sending.value}
            >
              An
            </button>
            <button
              type="button"
              onClick$={() => sendCommand('LED_OFF')}
              disabled={sending.value}
            >
              Aus
            </button>
          </div>
        </>
      )}
      {message.value && <p class="muted">{message.value}</p>}
    </div>
  );
});
