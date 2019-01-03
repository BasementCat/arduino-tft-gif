#ifndef _BUTTONS_IMPL_H_
#define _BUTTONS_IMPL_H_

class Buttons {
    public:
        Buttons(int in_l_pin, int in_m_pin, int in_r_pin) {
            l_pin = in_l_pin;
            m_pin = in_m_pin;
            r_pin = in_r_pin;

            pinMode(l_pin, INPUT_PULLUP);
            pinMode(m_pin, INPUT_PULLUP);
            pinMode(r_pin, INPUT_PULLUP);
        }

        void check() {
            l_set = digitalRead(l_pin) ? 0 : (l_set ?: millis());
            m_set = digitalRead(m_pin) ? 0 : (m_set ?: millis());
            r_set = digitalRead(r_pin) ? 0 : (r_set ?: millis());
            // Serial.println(l_set);
            // Serial.println(m_set);
            // Serial.println(r_set);
            // Serial.println('--------');
        }

        bool l_btn() { check(); return l_set ? ((millis() - l_set) > 10 ? true : false) : false; }
        bool m_btn() { check(); return m_set ? ((millis() - m_set) > 10 ? true : false) : false; }
        bool r_btn() { check(); return r_set ? ((millis() - r_set) > 10 ? true : false) : false; }

    private:
        int l_pin, m_pin, r_pin;
        unsigned int l_set, m_set, r_set;
};

#endif