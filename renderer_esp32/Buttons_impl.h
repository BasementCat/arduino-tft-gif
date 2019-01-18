#ifndef _BUTTONS_IMPL_H_
#define _BUTTONS_IMPL_H_

#define SET_TIME 10
#define READ_TIME 250

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
            now = millis();
            l_set = digitalRead(l_pin) ? 0 : (l_set ?: now);
            m_set = digitalRead(m_pin) ? 0 : (m_set ?: now);
            r_set = digitalRead(r_pin) ? 0 : (r_set ?: now);
            l_read = l_set ? l_read : 0;
            m_read = m_set ? m_read : 0;
            r_read = r_set ? r_read : 0;
            if ((l_read > 0) && ((now - l_read) > READ_TIME)) l_read = 0;
            if ((m_read > 0) && ((now - m_read) > READ_TIME)) m_read = 0;
            if ((r_read > 0) && ((now - r_read) > READ_TIME)) r_read = 0;
            // Serial.print("Set: ");Serial.print(l_set);Serial.print(m_set);Serial.println(r_set);
            // Serial.print("Read: ");Serial.print(l_read);Serial.print(m_read);Serial.println(r_read);
        }

        bool l_btn() {
            if ((l_set > 0) && ((now - l_set) > SET_TIME)) {
                if (l_read == 0) {
                    l_read = now;
                    return true;
                }
            }
            return false;
        }

        bool m_btn() {
            if ((m_set > 0) && ((now - m_set) > SET_TIME)) {
                if (m_read == 0) {
                    m_read = now;
                    return true;
                }
            }
            return false;
        }

        bool r_btn() {
            if ((r_set > 0) && ((now - r_set) > SET_TIME)) {
                if (r_read == 0) {
                    r_read = now;
                    return true;
                }
            }
            return false;
        }

    private:
        int l_pin, m_pin, r_pin;
        unsigned int now;
        unsigned int l_set, m_set, r_set;
        unsigned int l_read, m_read, r_read;
};

#endif