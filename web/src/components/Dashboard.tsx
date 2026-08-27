import { $, component$, useSignal, useVisibleTask$ } from '@qwik.dev/core';
import { apiGet } from '../lib/api';
import { getStoredToken, onTokenChanged, setStoredToken } from '../lib/token';
import CoapLightControl from './CoapLightControl';
import CoapTester from './CoapTester';
import CommissionerPanel from './CommissionerPanel';
import DeviceInfo from './DeviceInfo';
import NeighborList from './NeighborList';
import OtaBanner from './OtaBanner';
import SetupTokenTransfer from './SetupTokenTransfer';

const TOKEN_STORAGE_KEY = 'ot_br_setup_token';

export default component$(() => {
  const token = useSignal<string | null>(null);
  const setupModeActive = useSignal(false);
  const checking = useSignal(false);

  useVisibleTask$(() => {
    const stored = getStoredToken();
    if (stored) {
      token.value = stored;
    }
    const cleanup = onTokenChanged(() => {
      const updated = getStoredToken();
      token.value = updated || null;
    });
    return cleanup;
  });

  const checkAndFetchToken = $(async () => {
    checking.value = true;
    try {
      const status = await apiGet<{ setup_mode_active: boolean }>(
        '/setup/status',
      );
      setupModeActive.value = status.setup_mode_active;

      if (status.setup_mode_active) {
        const result = await apiGet<{ token: string }>('/setup/token');
        token.value = result.token;
        setStoredToken(result.token);
      }
    } catch (e) {
      console.error(e);
    } finally {
      checking.value = false;
    }
  });

  const resetToken = $(() => {
    localStorage.removeItem(TOKEN_STORAGE_KEY);
    window.dispatchEvent(new CustomEvent('ot-br-token-changed'));
  });

  if (token.value) {
    return (
      <div>
        <div class="token-status">
          <span>✓ Eingerichtet</span>
        </div>
        <OtaBanner />
        <DeviceInfo />
        <NeighborList />
        <CommissionerPanel />
        <CoapTester />
        <CoapLightControl />
        <SetupTokenTransfer>
          <button type="button" onClick$={resetToken} class="link-button">
            Zurücksetzen
          </button>
        </SetupTokenTransfer>
      </div>
    );
  }

  return (
    <div class="card setup-gate">
      <h2>Ersteinrichtung</h2>
      <p>
        Um dieses Gerät einzurichten, halte die <strong>BOOT-Taste</strong> am
        Gerät 3 Sekunden lang gedrückt. Du hast danach 10 Minuten Zeit für die
        Einrichtung.
      </p>
      <button
        type="button"
        onClick$={checkAndFetchToken}
        disabled={checking.value}
      >
        {checking.value ? 'Prüfe...' : 'Erneut prüfen'}
      </button>
      {!setupModeActive.value && !checking.value && (
        <p class="muted">Noch kein Einrichtungsmodus erkannt.</p>
      )}
      <SetupTokenTransfer />
    </div>
  );
});
