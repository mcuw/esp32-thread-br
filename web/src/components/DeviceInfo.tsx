import { component$, useSignal, useVisibleTask$ } from '@qwik.dev/core';
import { type DeviceInfo, apiGet } from '../lib/api';

export default component$(() => {
  const info = useSignal<DeviceInfo | null>(null);
  const error = useSignal('');

  useVisibleTask$(async () => {
    try {
      info.value = await apiGet<DeviceInfo>('/device/info');
    } catch (e) {
      error.value = e instanceof Error ? e.message : 'Unbekannter Fehler';
    }
  });

  return (
    <div class="card">
      <h2>Gerät</h2>
      {error.value && <p class="error">Fehler: {error.value}</p>}
      {!info.value && !error.value && <p>Lade...</p>}
      {info.value && (
        <dl>
          <dt>EUI-64</dt>
          <dd>
            <code>{info.value.eui64}</code>
          </dd>
          <dt>Firmware</dt>
          <dd>{info.value.firmware_version}</dd>
          <dt>Chip</dt>
          <dd>{info.value.chip_model}</dd>
          <dt>Freier Heap</dt>
          <dd>{Math.round(info.value.free_heap / 1024)} KB</dd>
        </dl>
      )}
    </div>
  );
});
