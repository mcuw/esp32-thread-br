import { $, component$, useSignal, useVisibleTask$ } from '@qwik.dev/core';
import {
  type BorderRoutingState,
  type SrpState,
  apiGet,
  apiGetAuth,
} from '../lib/api';
import { getStoredToken } from '../lib/token';

const ok = (v: boolean) => (v ? '✓' : '✗');

export default component$(() => {
  const srp = useSignal<SrpState | null>(null);
  const br = useSignal<BorderRoutingState | null>(null);
  const tlvs = useSignal('');
  const message = useSignal('');
  const lastUpdate = useSignal('');

  useVisibleTask$(({ cleanup }) => {
    const fetchState = async () => {
      try {
        const [s, b] = await Promise.all([
          apiGet<SrpState>('/thread/srp'),
          apiGet<BorderRoutingState>('/thread/border-routing'),
        ]);
        srp.value = s;
        br.value = b;
        lastUpdate.value = new Date().toLocaleTimeString();
      } catch (e) {
        console.error(e);
      }
    };
    fetchState();
    const interval = setInterval(fetchState, 10000);
    cleanup(() => clearInterval(interval));
  });

  const loadTlvs = $(async () => {
    try {
      const res = await apiGetAuth<{ tlvs: string }>(
        '/thread/dataset/tlvs',
        getStoredToken(),
      );
      tlvs.value = res.tlvs;
      message.value = '';
    } catch (e) {
      message.value = e instanceof Error ? e.message : 'Fehler';
    }
  });

  const copyTlvs = $(async () => {
    await navigator.clipboard.writeText(tlvs.value);
    message.value = 'Dataset kopiert';
  });

  const s = srp.value;
  const b = br.value;
  const matterHosts = s ? s.hosts.filter((h) => h.matter) : [];

  return (
    <div class="card">
      <h2>Matter</h2>
      {s && b ? (
        <table>
          <tbody>
            <tr>
              <td>Border Routing</td>
              <td>
                {ok(b.state === 'running')} {b.state}
              </td>
            </tr>
            <tr>
              <td>SRP-Server</td>
              <td>
                {ok(s.state === 'running')} {s.state} ({s.address_mode}, Port{' '}
                {s.port})
              </td>
            </tr>
            <tr>
              <td>OMR-Prefix</td>
              <td>
                <code>{b.favored_omr_prefix ?? '–'}</code>
              </td>
            </tr>
            <tr>
              <td>NAT64</td>
              <td>
                {ok(b.nat64.prefix_manager_state === 'Active')}{' '}
                {b.nat64.prefix_manager_state}{' '}
                <code>{b.nat64.favored_prefix ?? ''}</code>
              </td>
            </tr>
            <tr>
              <td>Matter-Geräte</td>
              <td>
                {s.matter_operational} in Betrieb, {s.matter_commissionable}{' '}
                bereit zur Inbetriebnahme
              </td>
            </tr>
          </tbody>
        </table>
      ) : (
        <p>Lade Status...</p>
      )}

      {matterHosts.length > 0 && (
        <table>
          <thead>
            <tr>
              <th>Host</th>
              <th>Node-ID</th>
              <th>Typ</th>
              <th>Adresse</th>
            </tr>
          </thead>
          <tbody>
            {matterHosts.flatMap((h) =>
              h.services
                .filter((svc) => svc.matter)
                .map((svc) => (
                  <tr key={`${h.name}/${svc.instance}`}>
                    <td>
                      <code>{h.name}</code>
                    </td>
                    <td>
                      <code>{svc.node_id ?? '–'}</code>
                    </td>
                    <td>{svc.matter}</td>
                    <td>
                      <code>{h.addresses[0] ?? ''}</code>
                    </td>
                  </tr>
                )),
            )}
          </tbody>
        </table>
      )}

      <h3>Thread-Dataset für Matter-Controller</h3>
      <p class="muted">
        Z. B.{' '}
        <code>
          chip-tool pairing ble-thread &lt;node-id&gt; hex:&lt;tlvs&gt;
          &lt;pin&gt; &lt;discriminator&gt;
        </code>
      </p>
      <button type="button" onClick$={loadTlvs}>
        Dataset (TLV) anzeigen
      </button>
      {tlvs.value && (
        <>
          <pre style="white-space: pre-wrap; word-break: break-all">
            {tlvs.value}
          </pre>
          <button type="button" onClick$={copyTlvs}>
            Kopieren
          </button>
        </>
      )}
      {message.value && <p class="muted">{message.value}</p>}
      <p class="muted">Aktualisiert: {lastUpdate.value}</p>
    </div>
  );
});
