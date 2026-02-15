import { useCallback, useContext, useEffect, useRef, useState } from 'preact/hooks';
import Card from '../../components/Card.jsx';
import { ApiServiceContext } from '../../services/ApiService.js';

export function Logs() {
  const apiService = useContext(ApiServiceContext);
  const [logContent, setLogContent] = useState('');
  const [tailing, setTailing] = useState(false);
  const [filter, setFilter] = useState('all');
  const logRef = useRef(null);
  const autoScrollRef = useRef(true);

  useEffect(() => {
    if (!tailing) return;

    const listenerId = apiService.on('evt:logs:tail', msg => {
      setLogContent(prev => {
        const updated = prev + (msg.content || '');
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
      // Clean up retry on unmount
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
  }, [apiService, tailing]);

  const handleScroll = useCallback(() => {
    if (!logRef.current) return;
    const { scrollTop, scrollHeight, clientHeight } = logRef.current;
    autoScrollRef.current = scrollHeight - scrollTop - clientHeight < 50;
  }, []);

  const filteredContent =
    filter === 'all'
      ? logContent
      : logContent
          .split('\n')
          .filter(line => {
            if (!line.trim()) return false;
            if (filter === 'error') return /\bERROR\b/.test(line);
            if (filter === 'warning') return /\b(?:ERROR|WARNING)\b/.test(line);
            if (filter === 'info') return /\b(?:ERROR|WARNING|INFO)\b/.test(line);
            return true;
          })
          .join('\n');

  return (
    <>
      <div className='mb-4 flex flex-row items-center gap-2'>
        <h2 className='flex-grow text-2xl font-bold sm:text-3xl'>Logs</h2>
      </div>

      <Card sm={12} title='Live Log Viewer'>
        <div className='flex flex-wrap items-center gap-2'>
          <button
            className={`btn btn-sm ${tailing ? 'btn-error' : 'btn-primary'}`}
            onClick={() => setTailing(!tailing)}
          >
            {tailing ? 'Stop' : 'Start'} Tail
          </button>
          <select
            className='select select-bordered select-sm'
            value={filter}
            onChange={e => setFilter(e.target.value)}
          >
            <option value='all'>All Levels</option>
            <option value='info'>Info+</option>
            <option value='warning'>Warning+</option>
            <option value='error'>Errors Only</option>
          </select>
          <button
            className='btn btn-outline btn-sm'
            onClick={() => setLogContent('')}
            disabled={!logContent}
          >
            Clear
          </button>
          {tailing && <span className='badge badge-success badge-sm animate-pulse'>Live</span>}
        </div>
        <div
          ref={logRef}
          onScroll={handleScroll}
          className='bg-base-300 mt-2 h-96 overflow-auto rounded p-3 font-mono text-xs leading-relaxed'
        >
          {filteredContent ? (
            <pre className='whitespace-pre-wrap break-all'>{filteredContent}</pre>
          ) : (
            <p className='text-base-content/40'>
              {tailing
                ? 'Waiting for log output...'
                : 'Click "Start Tail" to begin streaming logs.'}
            </p>
          )}
        </div>
      </Card>
    </>
  );
}
