% 1. Läs in data från filen som Python skapade
% Se till att du står i samma mapp i MATLAB som där .csv-filen ligger
rawData = readmatrix('lidar_data.csv');

% 2. Separera vinklar och avstånd
angles_deg = rawData(1:end,1) + 180;
distances = rawData(1:end,2);

% --- Härifrån kör du din befintliga visualiseringskod ---
validIdx = distances > 5 & distances < 200;
theta = deg2rad(angles_deg(validIdx) + 90);
r = distances(validIdx);

% 4. Skapa visualisering
figure('Name', 'LiDAR Scan Visualizer', 'Color', 'w');

% --- Vänster: Polär plot (hur sensorn ser världen) ---
subplot(1,2,1);
polarscatter(theta, r, 15, r, 'filled');
gca.ThetaZeroLocation = 'right';
colormap(jet);
title('Polär Scan (Rådata)');
ax = gca;
ax.ThetaZeroLocation = 'top'; % Justera så 0 grader är framåt

% --- Höger: Kartesisk plot (Rummets form i 2D) ---
subplot(1,2,2);
[x, y] = pol2cart(theta, r);
% scatter(x, y, 20, r, 'filled');
plot(x, y, '-o')
axis equal; % Viktigt för att skalan ska vara korrekt i 2D
grid on;
xlabel('X (mm)');
ylabel('Y (mm)');
title('2D Point Cloud (Kartesisk)');
hold on;
plot(0,0, 'rp', 'MarkerSize', 15, 'LineWidth', 2); % Markera sensorns position (0,0)
legend('Hinder', 'Sensor');














