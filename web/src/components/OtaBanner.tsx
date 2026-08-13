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
  error?: string;
  http_status?: number;
}

const GH_REPO = 'mcuw/esp32-thread-br';

export default component$(() => {
  const currentVersion = useSignal('');
  const latestRelease = useSignal<GhRelease | null>(null);
  const updateState = useSignal<
    'idle' | 'checking' | 'updating' | 'done' | 'error'
  >('idle');
  const token = useSignal('');
  const progress = useSignal(0);
  const lastError = useSignal('');

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
      updateState.value = 'error';
      lastError.value =
        'Firmware is currently being built on GitHub - please try again in a few minutes.';
      return;
    }

    updateState.value = 'updating';
    lastError.value = '';
    progress.value = 0;

    try {
      await apiPost('/ota/update', token.value, {
        url: asset.browser_download_url,
      });

      // Fortschritt pollen
      const poll = setInterval(async () => {
        const status = await apiGet<OtaStatus>('/ota/status');
        progress.value = status.progress_percent;

        if (status.state === 'success') {
          clearInterval(poll);
          updateState.value = 'done';

          // Nach dem Neustart des Geraets automatisch die Seite neu laden,
          // sobald es wieder erreichbar ist
          setTimeout(() => {
            const retryReload = setInterval(async () => {
              try {
                await apiGet<OtaStatus>('/ota/status');
                clearInterval(retryReload);
                location.reload();
              } catch {
                // Geraet noch nicht wieder erreichbar - weiter versuchen
              }
            }, 2000);
          }, 3000); // 3 Sek. initiale Wartezeit, bis das Geraet wirklich neu startet
        } else if (status.state === 'failed') {
          clearInterval(poll);
          updateState.value = 'error';
          lastError.value = status.error
            ? `${status.error}${status.http_status ? ` (HTTP ${status.http_status})` : ''}`
            : 'Unknown error occurred while updating firmware';
        }
      }, 1500);
    } catch (e) {
      updateState.value = 'error';
      lastError.value =
        e instanceof Error
          ? e.message
          : 'Unknown error occurred while starting the update';
    }
  });

  const isNewer =
    latestRelease.value &&
    latestRelease.value.tag_name !== currentVersion.value;
  const assetReady = latestRelease.value?.assets.some(
    (a) =>
      a.name.endsWith('.bin') &&
      !a.name.includes('bootloader') &&
      !a.name.includes('partition'),
  );

  if (!isNewer && updateState.value === 'idle') {
    return null;
  }

  return (
    <div class="banner">
      {isNewer && updateState.value === 'idle' && (
        <>
          <p>
            Update is available:{' '}
            <strong>{latestRelease.value?.tag_name}</strong> (current:{' '}
            {currentVersion.value})
          </p>
          {assetReady ? (
            <button type="button" onClick$={startUpdate}>
              Update now
            </button>
          ) : (
            <p class="muted">Firmware is still being built, please wait...</p>
          )}
        </>
      )}
      {updateState.value === 'updating' && (
        <div>
          <p>Update is running... {progress.value}%</p>
          <div class="progress-bar">
            <div
              class="progress-fill"
              style={{ width: `${progress.value}%` }}
            />
          </div>
        </div>
      )}
      {updateState.value === 'done' && (
        <p>Update success, device is restarting...</p>
      )}
      {updateState.value === 'error' && (
        <div>
          <p class="error">Failed to update firmware: {lastError.value}</p>
          <button type="button" onClick$={startUpdate}>
            Retry
          </button>
        </div>
      )}
    </div>
  );
});
