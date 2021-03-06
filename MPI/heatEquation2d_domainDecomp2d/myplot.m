function myplot(result,nx,ny)

L = 1.0;
W = 1.0;

[x,y] = meshgrid(linspace(0,L,ny),linspace(0,W,nx));
S = reshape(result(:,3),nx,ny);

h = surf(x,y,S,'EdgeColor','none'); %colormap hot;
axis equal;
axis tight;

%print('heat2d','-dpng')
