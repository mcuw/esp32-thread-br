import { $, component$, useSignal } from '@qwik.dev/core';
import { apiPost } from '../lib/api';

export default component$(() => {
  const status = useSignal<'idle' | 'starting' | 'active' | 'error'>('idle');
  const pskd = useSignal('J01NME');
  const eui64 = useSignal('*');
  const message = useSignal('');

  const openCommissioning = $(async () => {
    const token = localStorage.getItem('ot_br_setup_token') ?? '';
    status.value = 'starting';
    try {
      await apiPost('/thread/commissioner/open', token, { pskd: pskd.value });
      status.value = 'active';
      message.value = `Kommissionierung offen (PSKd: ${pskd.value}) - Gerät kann jetzt beitreten`;
    } catch (e) {
      status.value = 'error';
      message.value = e instanceof Error ? e.message : 'Fehler';
    }
  });

  return (
    <div class="card">
      <h2>Join a device</h2>
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
      <button
        type="button"
        onClick$={openCommissioning}
        disabled={status.value === 'starting'}
      >
        {status.value === 'starting' ? 'Open...' : 'Open Commissioning'}
      </button>

      {message.value && <p class="muted">{message.value}</p>}
    </div>
  );
});
