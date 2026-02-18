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

function StorageCard({ label, storage, info, isActive, onDelete }) {
  const [confirming, setConfirming] = useState(false);
  const hasFiles = info.size > 0 || info.oldSize > 0;

  if (!info.available && !hasFiles) return null;

  return (
    <Card sm={12} title={`${label} Logs`}>
      <div className='flex flex-col gap-3'>
        <div className='text-base-content/70 flex items-center gap-2 text-sm'>
          {isActive && <span className='badge badge-success badge-sm'>Active</span>}
          {!info.available && <span className='badge badge-warning badge-sm'>Unavailable</span>}
          <span>
            Path: <code className='text-xs'>{info.path}</code>
          </span>
          {isActive && (
            <span>
              {storage === 'sd'
                ? '(INFO+ level, 500 KB max with rotation)'
                : '(WARNING+ level, 50 KB max)'}
            </span>
          )}
        </div>
        {hasFiles ? (
          <div className='flex flex-wrap items-center gap-2'>
            <div className='flex items-center gap-2'>
              <span className='text-sm font-medium'>Current:</span>
              <span className='text-base-content/70 text-sm'>{formatBytes(info.size)}</span>
              <a
                href={`/api/logs/download?storage=${storage}`}
                target='_blank'
                rel='noopener'
                className={`btn btn-primary btn-sm ${info.size === 0 ? 'btn-disabled' : ''}`}
              >
                Download
              </a>
            </div>
            {info.oldSize > 0 && (
              <div className='flex items-center gap-2'>
                <span className='text-sm font-medium'>Previous:</span>
                <span className='text-base-content/70 text-sm'>{formatBytes(info.oldSize)}</span>
                <a
                  href={`/api/logs/download?storage=${storage}&old`}
                  target='_blank'
                  rel='noopener'
                  className='btn btn-outline btn-sm'
                >
                  Download
                </a>
              </div>
            )}
            <div className='flex items-center gap-2'>
              {!confirming ? (
                <button className='btn btn-error btn-outline btn-sm' onClick={() => setConfirming(true)}>
                  Delete
                </button>
              ) : (
                <>
                  <span className='text-warning text-sm'>Delete all {label} logs?</span>
                  <button
                    className='btn btn-error btn-sm'
                    onClick={() => {
                      setConfirming(false);
                      onDelete(storage);
                    }}
                  >
                    Confirm
                  </button>
                  <button className='btn btn-ghost btn-sm' onClick={() => setConfirming(false)}>
                    Cancel
                  </button>
                </>
              )}
            </div>
          </div>
        ) : (
          <p className='text-base-content/40 text-sm'>No log files found.</p>
        )}
      </div>
    </Card>
  );
}

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

  const handleDelete = useCallback(
    storage => {
      fetch(`/api/logs/delete?storage=${storage}`, { method: 'DELETE' })
        .then(() => fetchLogInfo())
        .catch(() => {});
    },
    [fetchLogInfo],
  );

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

  return (
    <>
      <div className='mb-4 flex flex-row items-center gap-2'>
        <h2 className='flex-grow text-2xl font-bold sm:text-3xl'>Logs</h2>
      </div>

      {logInfo ? (
        <>
          {logInfo.sd && (logInfo.sd.available || logInfo.sd.size > 0 || logInfo.sd.oldSize > 0) && (
            <div className='mb-4'>
              <StorageCard
                label='SD Card'
                storage='sd'
                info={logInfo.sd}
                isActive={logInfo.active === 'sd'}
                onDelete={handleDelete}
              />
            </div>
          )}
          {logInfo.spiffs && (logInfo.spiffs.size > 0 || logInfo.spiffs.oldSize > 0 || logInfo.active === 'spiffs') && (
            <div className='mb-4'>
              <StorageCard
                label='SPIFFS'
                storage='spiffs'
                info={logInfo.spiffs}
                isActive={logInfo.active === 'spiffs'}
                onDelete={handleDelete}
              />
            </div>
          )}
        </>
      ) : (
        <Card sm={12} title='Persistent Log Files'>
          <p className='text-base-content/40 text-sm'>Loading log file info...</p>
        </Card>
      )}

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
