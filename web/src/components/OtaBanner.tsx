import { $, component$, useSignal, useVisibleTask$ } from '@qwik.dev/core';
import { apiGet, apiPost } from '../lib/api';

interface GhRelease {
  tag_name: string;
  assets: { name: string; browser_download_url: string }[];
}

interface OtaStatus {
  current_version: string;
  state: string;
  progress_percent: number;
}

const GH_REPO = 'mcuw/esp32-thread-br';

export default component$(() => {
  const currentVersion = useSignal('');
  const latestRelease = useSignal<GhRelease | null>(null);
  const updateState = useSignal<
    'idle' | 'checking' | 'updating' | 'done' | 'error'
  >('idle');
  const token = useSignal('');

  useVisibleTask$(async () => {
    token.value = localStorage.getItem('ot_br_setup_token') ?? '';

    const status = await apiGet<OtaStatus>('/ota/status');
    currentVersion.value = status.current_version;

    const release = await fetch(
      `https://api.github.com/repos/${GH_REPO}/releases/latest`,
    ).then((r) => r.json());
    latestRelease.value = release;
  });

  const startUpdate = $(async () => {
    if (!latestRelease.value || !token.value) {
      return;
    }
    const asset = latestRelease.value.assets.find(
      (a) =>
        a.name.endsWith('.bin') &&
        !a.name.includes('bootloader') &&
        !a.name.includes('partition'),
    );
    if (!asset) {
      return;
    }

    updateState.value = 'updating';
    try {
      await apiPost('/ota/update', token.value, {
        url: asset.browser_download_url,
      });

      // Fortschritt pollen
      const poll = setInterval(async () => {
        const status = await apiGet<OtaStatus>('/ota/status');
        if (status.state === 'success') {
          clearInterval(poll);
          updateState.value = 'done';
        } else if (status.state === 'failed') {
          clearInterval(poll);
          updateState.value = 'error';
        }
      }, 2000);
    } catch (e) {
      updateState.value = 'error';
    }
  });

  const isNewer =
    latestRelease.value &&
    latestRelease.value.tag_name !== currentVersion.value;

  if (!isNewer) {
    return null;
  }

  return (
    <div class="banner">
      <p>
        Update verfügbar: <strong>{latestRelease.value?.tag_name}</strong>{' '}
        (aktuell: {currentVersion.value})
      </p>
      {updateState.value === 'idle' && (
        <button type="button" onClick$={startUpdate}>
          Jetzt aktualisieren
        </button>
      )}
      {updateState.value === 'updating' && <p>Update läuft...</p>}
      {updateState.value === 'done' && (
        <p>Update erfolgreich, Gerät startet neu...</p>
      )}
      {updateState.value === 'error' && (
        <p class="error">Update fehlgeschlagen.</p>
      )}
    </div>
  );
});
