import { useCallback, useContext, useEffect, useRef, useState } from 'preact/hooks';
import Card from '../../components/Card.jsx';
import { ApiServiceContext } from '../../services/ApiService.js';

function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

const LOG_SOURCES = [
  { tag: 'CTL', label: 'Core' },
  { tag: 'WEB', label: 'WebUI' },
  { tag: 'BLE', label: 'BLE Scale' },
  { tag: 'MQT', label: 'MQTT' },
  { tag: 'OTA', label: 'OTA' },
  { tag: 'SHT', label: 'Shot History' },
  { tag: 'PRF', label: 'Profiles' },
  { tag: 'SET', label: 'Settings' },
  { tag: 'DRV', label: 'Driver' },
  { tag: 'PLG', label: 'Plugins' },
  { tag: 'GND', label: 'Grind' },
  { tag: 'HMK', label: 'HomeKit' },
  { tag: 'BFL', label: 'Boiler Fill' },
  { tag: 'DNS', label: 'mDNS' },
  { tag: 'WKP', label: 'Wakeup' },
  { tag: 'LED', label: 'LED' },
];

export function Logs() {
  const apiService = useContext(ApiServiceContext);
  const [logContent, setLogContent] = useState('');
  const [paused, setPaused] = useState(false);
  const [filter, setFilter] = useState('info');
  const [source, setSource] = useState('all');
  const [logInfo, setLogInfo] = useState(null);
  const logRef = useRef(null);
  const autoScrollRef = useRef(true);
  const pausedRef = useRef(false);
  const pendingRef = useRef('');

  const fetchLogInfo = useCallback(() => {
    fetch('/api/logs/info')
      .then(r => r.json())
      .then(setLogInfo)
      .catch(() => setLogInfo(null));
  }, []);

  useEffect(() => {
    fetchLogInfo();
    const interval = setInterval(fetchLogInfo, 10000);
    return () => clearInterval(interval);
  }, [fetchLogInfo]);

  // Sync paused state to ref so event handler always sees current value
  useEffect(() => {
    pausedRef.current = paused;
    if (!paused && pendingRef.current) {
      const flushed = pendingRef.current;
      pendingRef.current = '';
      setLogContent(prev => {
        const updated = prev + flushed;
        return updated.length > 100000 ? updated.slice(-100000) : updated;
      });
    }
  }, [paused]);

  // Subscribe on mount, unsubscribe on unmount
  useEffect(() => {
    const listenerId = apiService.on('evt:logs:tail', msg => {
      const content = msg.content || '';
      if (pausedRef.current) {
        pendingRef.current += content;
        return;
      }
      setLogContent(prev => {
        const updated = prev + content;
        return updated.length > 100000 ? updated.slice(-100000) : updated;
      });
      if (autoScrollRef.current && logRef.current) {
        requestAnimationFrame(() => {
          if (logRef.current) {
            logRef.current.scrollTop = logRef.current.scrollHeight;
          }
        });
      }
    });

    // Retry subscribe until WebSocket is connected
    const trySend = () => {
      try {
        apiService.send({ tp: 'req:logs:subscribe' });
        return true;
      } catch (e) {
        return false;
      }
    };
    if (!trySend()) {
      const retryInterval = setInterval(() => {
        if (trySend()) clearInterval(retryInterval);
      }, 500);
      return () => {
        clearInterval(retryInterval);
        apiService.off('evt:logs:tail', listenerId);
        try { apiService.send({ tp: 'req:logs:unsubscribe' }); } catch (e) { /* noop */ }
      };
    }

    return () => {
      apiService.off('evt:logs:tail', listenerId);
      try { apiService.send({ tp: 'req:logs:unsubscribe' }); } catch (e) { /* noop */ }
    };
  }, [apiService]);

  const handleScroll = useCallback(() => {
    if (!logRef.current) return;
    const { scrollTop, scrollHeight, clientHeight } = logRef.current;
    autoScrollRef.current = scrollHeight - scrollTop - clientHeight < 50;
  }, []);

  const filteredContent =
    filter === 'all' && source === 'all'
      ? logContent
      : logContent
          .split('\n')
          .filter(line => {
            if (!line.trim()) return false;
            if (source !== 'all' && !line.includes(`[${source}]`)) return false;
            if (filter === 'error') return /\bERROR\b/.test(line);
            if (filter === 'warning') return /\b(?:ERROR|WARN)\b/.test(line);
            if (filter === 'info') return /\b(?:ERROR|WARN|INFO)\b/.test(line);
            return true;
          })
          .join('\n');

  const isSD = logInfo?.storage === 'sd';

  return (
    <>
      <div className='mb-4 flex flex-row items-center gap-2'>
        <h2 className='flex-grow text-2xl font-bold sm:text-3xl'>Logs</h2>
      </div>

      <Card sm={12} title='Persistent Log Files'>
        {logInfo ? (
          <div className='flex flex-col gap-3'>
            <div className='text-base-content/70 text-sm'>
              <p>
                Logs are saved to <span className='badge badge-sm'>{isSD ? 'SD Card' : 'SPIFFS'}</span>{' '}
                at <code className='text-xs'>{logInfo.path}</code>.{' '}
                {isSD
                  ? 'Recording INFO level and above. Max 500 KB per file with rotation.'
                  : 'Recording WARNING level and above (errors & warnings only). Max 50 KB to conserve flash storage.'}
              </p>
            </div>
            <div className='flex flex-wrap items-center gap-2'>
              <div className='flex items-center gap-2'>
                <span className='text-sm font-medium'>Current:</span>
                <span className='text-base-content/70 text-sm'>{formatBytes(logInfo.size)}</span>
                <a
                  href='/api/logs/download'
                  target='_blank'
                  rel='noopener'
                  className={`btn btn-primary btn-sm ${logInfo.size === 0 ? 'btn-disabled' : ''}`}
                >
                  Download
                </a>
              </div>
              {logInfo.oldSize > 0 && (
                <div className='flex items-center gap-2'>
                  <span className='text-sm font-medium'>Previous:</span>
                  <span className='text-base-content/70 text-sm'>{formatBytes(logInfo.oldSize)}</span>
                  <a
                    href='/api/logs/download?old'
                    target='_blank'
                    rel='noopener'
                    className='btn btn-outline btn-sm'
                  >
                    Download
                  </a>
                </div>
              )}
            </div>
          </div>
        ) : (
          <p className='text-base-content/40 text-sm'>Loading log file info...</p>
        )}
      </Card>

      <div className='mt-4' />

      <Card sm={12} title='Live Log Viewer'>
        <div className='flex flex-wrap items-center gap-2'>
          <button
            className={`btn btn-sm ${paused ? 'btn-primary' : 'btn-warning'}`}
            onClick={() => setPaused(!paused)}
          >
            {paused ? 'Resume' : 'Pause'}
          </button>
          <select
            className='select select-bordered select-sm'
            value={filter}
            onChange={e => setFilter(e.target.value)}
          >
            <option value='all'>All (Debug+)</option>
            <option value='info'>Info+</option>
            <option value='warning'>Warning+</option>
            <option value='error'>Errors Only</option>
          </select>
          <select
            className='select select-bordered select-sm'
            value={source}
            onChange={e => setSource(e.target.value)}
          >
            <option value='all'>All Sources</option>
            {LOG_SOURCES.map(s => (
              <option key={s.tag} value={s.tag}>{s.label}</option>
            ))}
          </select>
          <button
            className='btn btn-outline btn-sm'
            onClick={() => {
              setLogContent('');
              try { apiService.send({ tp: 'req:logs:clear' }); } catch (e) { /* noop */ }
            }}
            disabled={!logContent}
          >
            Clear
          </button>
          {!paused && <span className='badge badge-success badge-sm animate-pulse'>Live</span>}
          {paused && <span className='badge badge-warning badge-sm'>Paused</span>}
        </div>
        <p className='text-base-content/50 mt-1 text-xs'>
          Streams recent logs from an 8 KB buffer — includes history since last boot or buffer capacity. Defaults to Info+.
        </p>
        <div
          ref={logRef}
          onScroll={handleScroll}
          className='bg-base-300 mt-2 h-96 overflow-auto rounded p-3 font-mono text-xs leading-relaxed'
        >
          {filteredContent ? (
            <pre className='whitespace-pre-wrap break-all'>{filteredContent}</pre>
          ) : (
            <p className='text-base-content/40'>Waiting for log output...</p>
          )}
        </div>
      </Card>
    </>
  );
}
