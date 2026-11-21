package ssatr.exemple.mqtt;

import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;

/**
 * PUBLISHER MQTT
 *
 * Se conectează la broker și publică 10 mesaje pe un topic, câte unul pe secundă.
 *
 * Porniți întâi MqttSubscriberExample, apoi acest program.
 */
public class MqttPublisherExample {

    public static void main(String[] args) throws MqttException, InterruptedException {
        // fiecare client conectat la broker trebuie să aibă un ID unic
        String clientId = MqttConfig.STUDENT + "-publisher-" + System.currentTimeMillis();
        MqttClient client = new MqttClient(MqttConfig.BROKER, clientId, new MemoryPersistence());

        MqttConnectOptions options = new MqttConnectOptions();
        options.setCleanSession(true);
        options.setAutomaticReconnect(true);

        System.out.println("Conectare la " + MqttConfig.BROKER + " ...");
        client.connect(options);
        System.out.println("Conectat.");

        for (int i = 1; i <= 1000; i++) {
            // conținutul mesajului: text simplu
            String payload = "Salut #" + i;

            MqttMessage message = new MqttMessage(payload.getBytes());
            message.setQos(1);   // 0 = cel mult o dată, 1 = cel puțin o dată, 2 = exact o dată

            client.publish(MqttConfig.TOPIC, message);
            System.out.println("Publicat pe " + MqttConfig.TOPIC + ": " + payload);

            Thread.sleep(1000);
        }

        client.disconnect();
        client.close();
        System.out.println("Deconectat.");
    }
}
