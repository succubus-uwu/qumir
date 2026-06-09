let markedPromise = null;

export function loadMarked() {
  markedPromise ||= import('https://cdn.jsdelivr.net/npm/marked@17.0.1/lib/marked.esm.js')
    .then(module => module.marked);
  return markedPromise;
}
