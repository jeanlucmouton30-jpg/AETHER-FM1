/* SPDX-License-Identifier: GPL-3.0-only
 * GR!MM - AETHER HYBRID DRONE
 *
 * Premier noyau sonore :
 *   BLEND   : mélange oscillateur / spectre harmonique
 *   MOTION  : modulation lente de la phase
 *   DENSITY : nombre d'harmoniques
 *   SPACE   : dispersion des phases harmoniques
 *
 * La voix réservée au drone est gérée séparément dans grimm.c.
 */

#define DRONE_LV 9000
#define DRONE_HMAX 12u

/* Démarrage : les deux phases repartent de zéro. */
static void drone_begin(voice_t *v)
{
    v->ph[0] = 0;
    v->ph[1] = 0;
}

/* Rendu d'un bloc mono de CTL échantillons. */
static void drone_block(voice_t *v, const volatile int32_t *p,
                        int32_t *mono, int32_t amp0, int32_t amp1)
{
    uint32_t i, h;
    uint32_t ph = v->ph[0];
    uint32_t modph = v->ph[1];
    uint32_t inc = vinc(VP16(v));
    uint32_t motion = (uint32_t)clamp(p[1], 0, 127);
    uint32_t blend = (uint32_t)clamp(p[0], 0, 127);
    uint32_t density = (uint32_t)clamp(p[2], 0, 127);
    uint32_t space = (uint32_t)clamp(p[3], 0, 127);
    uint32_t harmonics = 1u + density * (DRONE_HMAX - 1u) / 127u;
    uint32_t modinc = 0x00001000u + motion * 0x00008000u / 127u;
    uint32_t norm = 0;
    int32_t weights[DRONE_HMAX];
    int32_t a0 = amp_lv(amp0, DRONE_LV);
    int32_t da = amp_lv(amp1, DRONE_LV) - a0;

    for (h = 1; h <= harmonics; h++) norm += 32767u / h;

    /* Les poids ne changent pas pendant le bloc audio. */
    for (h = 1; h <= harmonics; h++) {
        uint32_t raw_weight = 32767u / h;
        weights[h - 1u] = (int32_t)((32767u * raw_weight) / norm);
    }

    for (i = 0; i < (uint32_t)CTL; i++) {
        int32_t lfo = sine_i(modph);
        uint32_t shifted = ph +
            (uint32_t)(((int64_t)lfo * motion * 4096) / 127);
        int32_t analog = sine_i(shifted);
        int64_t sum = 0;
        int32_t spectral, sample;

        /* Banque additive : les harmoniques élevées sont progressivement atténuées. */
        for (h = 1; h <= harmonics; h++) {
            uint32_t hp = shifted * h +
                (uint32_t)(((int64_t)lfo * space * (h - 1u) * 512) / 127);
            sum += ((int64_t)sine_i(hp) * weights[h - 1u]) >> 15;
        }

        spectral = clamp((int32_t)sum, -32767, 32767);

        /* BLEND : 0 = analogique, 127 = spectral. */
        sample = (int32_t)(((int64_t)analog * (127u - blend) +
                            (int64_t)spectral * blend) / 127);

        mono[i] += vca(sample, amp_at(a0, da, (int32_t)i));

        ph += inc;
        modph += modinc;
    }

    v->ph[0] = ph;
    v->ph[1] = modph;
}

/* Valeurs affichées sur la page SOUND. */
static void drone_text(const volatile int32_t *p, uint32_t k, char *s)
{
    uint32_t v = (uint32_t)clamp(p[k], 0, 127);

    switch (k) {
    case 0:
        put_s(put_u(s, (v * 100u + 63u) / 127u), "%");
        return;
    case 1:
        put_s(put_u(s, v), "%");
        return;
    case 2:
        put_s(put_u(s, 1u + v * (DRONE_HMAX - 1u) / 127u), " HARM");
        return;
    default:
        put_s(put_u(s, v), "%");
        return;
    }
}

static const engine_t DRONE_ENGINE = {
    "HYBRID DRONE",
    {"BLEND", "MOTION", "DENSITY", "SPACE"},
    {127, 127, 127, 127},
    drone_begin, drone_block, drone_text
};
