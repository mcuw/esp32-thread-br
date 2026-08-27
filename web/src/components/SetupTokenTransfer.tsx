import {
  $,
  Slot,
  component$,
  useSignal,
  useVisibleTask$,
} from '@qwik.dev/core';
import { getStoredToken, setStoredToken } from '../lib/token';

export default component$(() => {
  const mode = useSignal<'idle' | 'show' | 'scan'>('idle');
  const currentToken = useSignal('');
  const qrCanvasRef = useSignal<HTMLCanvasElement>();
  const scanCanvasRef = useSignal<HTMLCanvasElement>();
  const videoRef = useSignal<HTMLVideoElement>();
  const manualInput = useSignal('');
  const scanError = useSignal('');
  const scanSuccess = useSignal(false);

  const streamRef = useSignal<MediaStream | null>();
  const scanIntervalRef = useSignal<number>();

  useVisibleTask$(() => {
    currentToken.value = getStoredToken();
  });

  // --- show QR-Code ---
  const showQr = $(async () => {
    mode.value = 'show';
    currentToken.value = getStoredToken();

    if (!currentToken.value) {
      return;
    }

    const QRCode = await import('qrcode');
    // Kurzer Delay, damit das Canvas-Element im DOM ist, bevor wir zeichnen
    setTimeout(async () => {
      if (qrCanvasRef.value) {
        await QRCode.toCanvas(qrCanvasRef.value, currentToken.value, {
          width: 160,
          margin: 2,
        });
      }
    }, 0);
  });

  // --- QR-Code per Kamera scannen ---
  const startScan = $(async () => {
    mode.value = 'scan';
    scanError.value = '';
    scanSuccess.value = false;

    if (!navigator.mediaDevices?.getUserMedia) {
      scanError.value =
        'Camera-Access is not supported (HTTPS is required). Please enter manually.';
      return;
    }

    try {
      streamRef.value = await navigator.mediaDevices.getUserMedia({
        video: { facingMode: 'environment' },
      });
      if (videoRef.value) {
        videoRef.value.srcObject = streamRef.value;
        await videoRef.value.play();
      }

      const jsQR = (await import('jsqr')).default;

      scanIntervalRef.value = window.setInterval(() => {
        if (!videoRef.value || !scanCanvasRef.value) {
          return;
        }
        const video = videoRef.value;
        const canvas = scanCanvasRef.value;
        if (video.videoWidth === 0) {
          return;
        }

        canvas.width = video.videoWidth;
        canvas.height = video.videoHeight;
        const ctx = canvas.getContext('2d');
        if (!ctx) {
          return;
        }

        ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
        const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
        const result = jsQR(imageData.data, imageData.width, imageData.height);

        if (result?.data) {
          setStoredToken(result.data);
          currentToken.value = result.data;
          scanSuccess.value = true;
          window.dispatchEvent(new CustomEvent('ot-br-token-changed'));
          stopScan();
        }
      }, 300);
    } catch (e) {
      scanError.value =
        'Camera-Access denied or not available. Please enter manually.';
    }
  });

  const stopScan = $(() => {
    if (scanIntervalRef.value) {
      clearInterval(scanIntervalRef.value);
      scanIntervalRef.value = undefined;
    }
    if (streamRef.value) {
      for (const track of streamRef.value.getTracks()) {
        track.stop();
      }

      streamRef.value = null;
    }
  });

  const closePanel = $(() => {
    stopScan();
    mode.value = 'idle';
  });

  const saveManualToken = $(() => {
    if (manualInput.value.trim().length === 0) {
      return;
    }
    setStoredToken(manualInput.value.trim());
    currentToken.value = manualInput.value.trim();
    manualInput.value = '';
    scanSuccess.value = true;
    window.dispatchEvent(new CustomEvent('ot-br-token-changed'));
    mode.value = 'idle';
  });

  return (
    <div class="card">
      <h2>Setup-Token</h2>

      {mode.value === 'idle' && (
        <div class="button-row">
          <button type="button" onClick$={showQr}>
            Show QR-Code
          </button>
          <button type="button" onClick$={startScan}>
            Scan QR-Code
          </button>
          <Slot />
        </div>
      )}

      {mode.value === 'show' && (
        <div>
          {currentToken.value ? (
            <>
              <canvas ref={qrCanvasRef} />
              <div>{currentToken.value}</div>
              <p class="muted">
                Scan with another device to migrate the Setup-Token.
              </p>
            </>
          ) : (
            <p class="muted">
              The Setup-Token does not exist. Please setup with the BOOT-button.
            </p>
          )}
          <button type="button" onClick$={closePanel}>
            Close
          </button>
        </div>
      )}

      {mode.value === 'scan' && (
        <div>
          {!scanError.value && !scanSuccess.value && (
            <>
              <video
                ref={videoRef}
                muted
                playsInline
                style={{ width: '100%', maxWidth: '320px' }}
              />
              <canvas ref={scanCanvasRef} style={{ display: 'none' }} />
              <p class="muted">Set the camera on the QR-Code...</p>
            </>
          )}

          {scanError.value && (
            <div>
              <p class="error">{scanError.value}</p>
              <label>
                Enter the Token manually
                <input
                  type="text"
                  value={manualInput.value}
                  onInput$={(e) => {
                    manualInput.value = (e.target as HTMLInputElement).value;
                  }}
                  placeholder="enter the Setup-Token"
                />
              </label>
              <button type="button" onClick$={saveManualToken}>
                Speichern
              </button>
            </div>
          )}

          {scanSuccess.value && <p>Token successfully set ✓</p>}

          <button type="button" onClick$={closePanel}>
            Close
          </button>
        </div>
      )}
    </div>
  );
});
