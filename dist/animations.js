/* Dark Era TCG — animation helpers.
   Server HTML is swapped in by htmx; this file only animates what changed.
   1) FLIP: cards keep their id (card-<id>) across swaps, so we measure before/after
      and slide them from the old place to the new one (battle -> victory / graveyard).
   2) Toast auto-hide. */
(() => {
  const reduce = matchMedia('(prefers-reduced-motion: reduce)').matches;
  let before = new Map();

  const snapshot = () => {
    before = new Map();
    document.querySelectorAll('.card[data-card-id]').forEach(el => {
      before.set(el.dataset.cardId, el.getBoundingClientRect());
    });
  };

  const play = () => {
    if (reduce) return;
    document.querySelectorAll('.card[data-card-id]').forEach(el => {
      const from = before.get(el.dataset.cardId);
      if (!from) return;
      const to = el.getBoundingClientRect();
      const dx = from.left - to.left, dy = from.top - to.top;
      if (!dx && !dy) return;
      el.animate(
        [{ transform: `translate(${dx}px,${dy}px) scale(${from.width / to.width})`, zIndex: 5 },
         { transform: 'none', zIndex: 5 }],
        { duration: 650, easing: 'cubic-bezier(.2,.8,.2,1)' }
      );
    });
    // Cards that left the DOM because they went to the graveyard cannot be tracked;
    // pulse the pile instead so the player sees where they went.
    document.querySelectorAll('.pile.grave .stack').forEach(el => {
      if (el.textContent.trim() && el.dataset.n !== el.textContent) {
        el.dataset.n = el.textContent;
        el.animate([{ transform: 'scale(1.18)' }, { transform: 'scale(1)' }], { duration: 400 });
      }
    });
  };

  // Fires for every websocket frame, before and after htmx swaps it in
  document.body.addEventListener('htmx:wsBeforeMessage', snapshot);
  document.body.addEventListener('htmx:wsAfterMessage', () => requestAnimationFrame(play));

  // Toast: hide after 2.2s
  new MutationObserver(() => {
    const t = document.getElementById('toast');
    if (t && t.classList.contains('show')) {
      clearTimeout(t._h);
      t._h = setTimeout(() => t.classList.remove('show'), 2200);
    }
  }).observe(document.body, { childList: true, subtree: true, attributes: true, attributeFilter: ['class'] });
})();
