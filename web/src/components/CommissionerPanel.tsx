import { $, component$, useSignal } from '@qwik.dev/core';
import { apiPost } from '../lib/api';

export default component$(() => {
  const status = useSignal<'idle' | 'starting' | 'active' | 'error'>('idle');
  const pskd = useSignal('J01NME');
  const eui64 = useSignal('*');
  const message = useSignal('');

  const startCommissioner = $(async () => {
    const token = localStorage.getItem('ot_br_setup_token') ?? '';
    status.value = 'starting';
    try {
      await apiPost('/thread/commissioner/start', token);
      status.value = 'active';
      message.value = 'Commissioner active - Joiner can now join';
    } catch (e) {
      status.value = 'error';
      message.value =
        e instanceof Error ? e.message : 'Error starting commissioner';
    }
  });

  const addJoiner = $(async () => {
    const token = localStorage.getItem('ot_br_setup_token') ?? '';
    try {
      await apiPost('/thread/commissioner/joiner', token, {
        eui64: eui64.value,
        pskd: pskd.value,
      });
      message.value = `Joiner added (PSKd: ${pskd.value}) - 2 minute window`;
    } catch (e) {
      message.value = e instanceof Error ? e.message : 'Error adding joiner';
    }
  });

  return (
    <div class="card">
      <h2>Join a device</h2>
      {status.value !== 'active' && (
        <button
          type="button"
          onClick$={startCommissioner}
          disabled={status.value === 'starting'}
        >
          {status.value === 'starting' ? 'Starte...' : 'Commissioner starten'}
        </button>
      )}
      {status.value === 'active' && (
        <div>
          <label>
            Joiner-Passcode (PSKd)
            <input
              type="text"
              value={pskd.value}
              onInput$={(e) => {
                pskd.value = (e.target as HTMLInputElement).value;
              }}
            />
          </label>
          <button type="button" onClick$={addJoiner}>
            Allow device to join
          </button>
        </div>
      )}
      {message.value && <p class="muted">{message.value}</p>}
    </div>
  );
});
