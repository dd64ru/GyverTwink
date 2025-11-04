void switchEff() {
  // поиск favorite эффектов
  while (true) {
    if (cfg.rndCh) curEff = random(0, TOTAL_EFFECTS);
    else {
      if (++curEff >= TOTAL_EFFECTS) curEff = 0;
    }
    if (effs[curEff].fav) break;
  }
  DEBUG("switch to: ");
  DEBUGLN(curEff);
}
